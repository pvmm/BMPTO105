from __future__ import annotations

import numpy as np

from bmpto105.approximator import Approximator


# constants
TILE_SIZE = 8
RGB_SIZE = 3
TILE_BYTES = TILE_SIZE * TILE_SIZE * RGB_SIZE
COLORS_PER_ROW = 4


class KMC(Approximator):
    """Approximate an RGB 8x8 tile using k-means clustering per row."""

    def __init__(self, max_iterations: int = 20) -> None:
        """
        Initialize K-Means Clustering algorithm with default parameters

        Args:
            max_iterations:
                Maximum number of k-means iterations per row (default = 20).
        """
        self.max_iterations = max_iterations


    def approximate_tile(self, rgb_data: bytes) -> bytes:
        """
        Approximate an 8x8 RGB tile using four colors per row.

        Each of the eight rows is processed independently using
        k-means with up to four colors.

        Therefore, a tile can contain up to 32 color entries:

            8 rows × 4 colors = 32 colors

        Args:
            rgb_data:
                Exactly 192 bytes containing 64 RGB pixels in
                row-major order:

                    R G B R G B ...

        Returns:
            Exactly 192 bytes containing the approximated RGB tile.
        """
        if len(rgb_data) != TILE_BYTES:
            raise ValueError(
                f"Expected {TILE_BYTES} bytes for an 8x8 RGB tile, "
                f"got {len(rgb_data)}"
            )

        # 192 bytes -> 8 rows × 8 pixels × 3 channels.
        pixels = np.frombuffer(
            rgb_data,
            dtype=np.uint8,
        ).reshape(8, 8, 3).astype(np.float32)

        result = np.empty_like(pixels)

        for y in range(8):
            row = pixels[y]
            result[y] = self._approximate_row(row)

        return result.astype(np.uint8).tobytes()


    def _approximate_row(self, row: np.ndarray) -> np.ndarray:
        """
        Approximate one 8-pixel RGB row using up to four colors.
        """

        # 8 pixels × 3 channels -> 8 × 3
        pixels = row.reshape(8, 3)

        # Only distinct colors can be useful as centroids.
        unique = np.unique(pixels, axis=0)

        k = min(COLORS_PER_ROW, len(unique))

        if k == 1:
            return np.repeat(
                unique[0][None, :],
                8,
                axis=0,
            )

        # ---------------------------------------------------------
        # Deterministic farthest-point initialization
        # ---------------------------------------------------------

        centroids = np.empty(
            (k, 3),
            dtype=np.float32,
        )

        # First centroid is deterministic.
        centroids[0] = unique[0]

        # Distance from every unique color to its nearest centroid.
        min_distances = np.sum(
            (unique - centroids[0]) ** 2,
            axis=1,
        )

        for i in range(1, k):
            index = np.argmax(min_distances)

            centroids[i] = unique[index]

            distances = np.sum(
                (unique - centroids[i]) ** 2,
                axis=1,
            )

            min_distances = np.minimum(
                min_distances,
                distances,
            )

        # ---------------------------------------------------------
        # K-means
        # ---------------------------------------------------------

        for _ in range(self.max_iterations):
            distances = np.sum(
                (
                    pixels[:, None, :]
                    - centroids[None, :, :]
                ) ** 2,
                axis=2,
            )

            labels = np.argmin(
                distances,
                axis=1,
            )

            new_centroids = centroids.copy()

            for cluster in range(k):
                members = pixels[labels == cluster]

                if len(members):
                    new_centroids[cluster] = members.mean(axis=0)

            if np.allclose(
                centroids,
                new_centroids,
            ):
                centroids = new_centroids
                break

            centroids = new_centroids

        # Assign pixels using the final centroids.
        distances = np.sum(
            (
                pixels[:, None, :]
                - centroids[None, :, :]
            ) ** 2,
            axis=2,
        )

        labels = np.argmin(
            distances,
            axis=1,
        )

        return centroids[labels]
