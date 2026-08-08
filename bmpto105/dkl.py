from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from bmpto105.approximator import Approximator


TILE_SIZE = 8
RGB_SIZE = 3
TILE_BYTES = TILE_SIZE * TILE_SIZE * RGB_SIZE


class DKL(Approximator):
    """
    Despeckle an RGB 8x8 tile while preserving the independent
    4-colors-per-row structure of the tile format.
    """

    threshold: int
    min_neighbors: int

    def __init__(self, threshold: int | None, min_neighbors: int | None) -> None:
        """
        Constructor

        Args:
            threshold:
                Maximum Euclidean RGB distance for two colors to be
                considered equivalent.

            min_neighbors:
                Minimum number of nearby pixels supporting the
                replacement color.
        """
        threshold = 30 if threshold is None else threshold
        if threshold < 0:
            raise ValueError("threshold must be non-negative")
        self.threshold = threshold

        min_neighbors = 2 if min_neighbors is None else min_neighbors
        if not 1 <= min_neighbors <= 4:
            raise ValueError("min_neighbors must be between 1 and 4")
        self.min_neighbors = min_neighbors


    def approximate_tile(self, rgb_data: bytes) -> bytes:
        """
        Despeckle an 8x8 RGB tile.

        Each row is processed independently. A pixel is replaced when
        its color is significantly different from a sufficiently large
        group of nearby pixels in the same row.

        No colors are created: a replaced pixel receives an existing
        color from its row.

        Args:
            rgb_data:
                Exactly 192 bytes containing 64 RGB pixels:

                    R G B R G B ...

        Returns:
            Exactly 192 bytes containing the despeckled tile.
        """
        if len(rgb_data) != TILE_BYTES:
            raise ValueError(
                f"Expected {TILE_BYTES} bytes for an 8x8 RGB tile, "
                f"got {len(rgb_data)}"
            )


        pixels: NDArray[np.float32] = np.frombuffer(
            rgb_data,
            dtype=np.uint8,
        ).reshape(8, 8, 3).astype(np.float32)

        result: NDArray[np.float32] = pixels.copy()

        for y in range(8):
            row: NDArray[np.float32] = pixels[y]

            for x in range(8):
                pixel = row[x]

                # Look only at pixels in the same row.
                #
                # We use a radius of two, giving up to four neighbors:
                #
                #     x-2  x-1  [x]  x+1  x+2
                #
                neighbors = []

                for dx in (-2, -1, 1, 2):
                    nx = x + dx

                    if 0 <= nx < 8:
                        neighbors.append(row[nx])

                if not neighbors:
                    continue

                neighbors_array: NDArray[np.float32] = np.asarray(
                    neighbors,
                    dtype=np.float32,
                )

                # Find groups of similar colors among the neighbors.
                #
                # Each neighbor votes for all other neighbors within
                # the RGB threshold.
                distances: NDArray[np.float64] = np.linalg.norm(
                    neighbors_array[:, None, :] -
                    neighbors_array[None, :, :],
                    axis=2,
                )

                similarity_counts: NDArray[np.int64] = np.sum(
                    distances <= self.threshold,
                    axis=1,
                )

                dominant_index: int = int(np.argmax(similarity_counts))
                dominant_color = neighbors_array[dominant_index]

                # Collect all neighbors belonging to the dominant
                # color group.
                dominant_distances: NDArray[np.float32] = np.linalg.norm(
                    neighbors_array - dominant_color,
                    axis=1,
                )

                supporting_neighbors = neighbors_array[
                    dominant_distances <= self.threshold
                ]

                if len(supporting_neighbors) < self.min_neighbors:
                    continue

                # Determine whether the current pixel is actually
                # different from the dominant group.
                pixel_distance = np.linalg.norm(
                    supporting_neighbors - pixel,
                    axis=1,
                )

                # If the pixel is already compatible with the dominant
                # group, it is not a speckle.
                if np.all(pixel_distance <= self.threshold):
                    continue

                # Use an existing color rather than averaging colors.
                #
                # Pick the supporting neighbor closest to the dominant
                # color. This guarantees that the replacement is an
                # actual color already present in the row.
                replacement_index: int = int(np.argmin(
                    np.linalg.norm(
                        supporting_neighbors - dominant_color,
                        axis=1,
                    ))
                )

                result[y, x] = supporting_neighbors[
                    replacement_index
                ]

        return result.astype(np.uint8).tobytes()
