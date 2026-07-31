import numpy as np

from typing import Union
from bmpto105.approximator import Approximator


class SVD(Approximator):
    def __init__(self, threshold: float = 0.0) -> None:
        """
        Process tiles sing single value decomposition (SVD)

        Args:
            threshold: compression level (float: 0-1)
        """
        self.threshold: float = threshold

    def tile_to_matrix(self, tile: bytes) -> np.ndarray:
        """
        converts tile to a 8x8x3 matrix of 64 bytes
        """
        # Converte bytes para array de uint8
        pixels: np.ndarray = np.frombuffer(tile, dtype=np.uint8)
        # Reshape para 8x8x3
        return pixels.reshape(8, 8, 3)

    def matrix_to_tile(self, matrix: np.ndarray) -> bytes:
        """
        convert 8x8x3 matrix back to bytes
        """
        # Garante que os valores estão no intervalo [0, 255]
        matrix = np.clip(matrix, 0, 255).astype(np.uint8)
        # Converte para bytes
        return matrix.tobytes()

    def approximate_tile(self, tile: bytes) -> bytes:
        """
        applies low rank approximation using truncated SVD
        """
        # convert tile into a matrix
        matrix: np.ndarray = self.tile_to_matrix(tile)

        # process each RGB channel separately
        approximated: np.ndarray = np.zeros_like(matrix, dtype=np.float32)

        for channel in range(3):
            # extract channel
            channel_matrix: np.ndarray = matrix[:, :, channel].astype(np.float32)

            # apply SVD
            U: np.ndarray
            s: np.ndarray
            Vt: np.ndarray
            U, s, Vt = np.linalg.svd(channel_matrix, full_matrices=False)

            # calculate number of singular values to maintain based on threshold
            s_total: np.float64 = np.sum(s)
            if s_total > 0:
                s_cumsum: np.ndarray = np.cumsum(s) / s_total
                k: Union[int, np.integer] = np.searchsorted(s_cumsum, 1.0 - self.threshold) + 1
                k = max(1, min(k, len(s)))  # Mantém pelo menos 1 e no máximo todos
            else:
                k = 1

            k_int: int = int(k) if isinstance(k, (int, np.integer)) else k
            s_truncated: np.ndarray = np.zeros_like(s)
            s_truncated[:k_int] = s[:k_int]

            approximated_channel: np.ndarray = U @ np.diag(s_truncated) @ Vt

            # stores approximate channel
            approximated[:, :, channel] = approximated_channel

        # converts back to tile
        return self.matrix_to_tile(approximated)
