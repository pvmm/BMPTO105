import numpy as np

from typing import Any
from scipy.fftpack import dct, idct


class DCT:
    def __init__(self, threshold: float = 0.1, keep_coeffs: int | None = None) -> None:
        '''
        Processes tiles using DCT

        Args:
            threshold: threshold for coefficient disposal (0-1)
            keep_coeffs: fixed number of coefficients to maintain (overwrites threshold)
        '''
        self.threshold = threshold
        self.keep_coeffs = keep_coeffs

    def dct2(self, block: np.ndarray) -> Any: #np.ndarray[tuple[Any, ...], np.dtype[Any]]:
        '''applies 2D DCT'''
        return dct(dct(block.T, norm='ortho').T, norm='ortho')

    def idct2(self, block: np.ndarray) -> np.ndarray:
        '''applies inverse 2D DCT'''
        return idct(idct(block.T, norm='ortho').T, norm='ortho')

    def approximate_tile(self, tile: bytes) -> bytes:
        '''applies truncated DCT for approximation'''
        # conver to matrix
        matrix = np.frombuffer(tile, dtype=np.uint8).reshape(8, 8, 3)
        approximated = np.zeros_like(matrix, dtype=np.float32)

        for channel in range(3):
            # Extrai canal
            channel_data = matrix[:, :, channel].astype(np.float32)

            # applies DCT
            dct_coeffs = self.dct2(channel_data)

            # create a mask for maintaining coefficients
            mask = np.ones_like(dct_coeffs)

            if self.keep_coeffs is not None:
                # maintain the biggest K coefficients in absolute values
                flat_coeffs = np.abs(dct_coeffs.flatten())
                indices = np.argsort(flat_coeffs)[-self.keep_coeffs:]
                mask = np.zeros_like(dct_coeffs)
                mask.flat[indices] = 1
            else:
                # use threshold based on total energy
                total_energy = np.sum(dct_coeffs ** 2)
                if total_energy > 0:
                    # order coefficients by energy
                    sorted_coeffs = np.sort(np.abs(dct_coeffs.flatten()))[::-1]
                    cumsum = np.cumsum(sorted_coeffs ** 2) / total_energy
                    k = np.searchsorted(cumsum, 1.0 - self.threshold) + 1

                    # maintain K-biggest
                    flat_coeffs = np.abs(dct_coeffs.flatten())
                    indices = np.argsort(flat_coeffs)[-k:]
                    mask = np.zeros_like(dct_coeffs)
                    mask.flat[indices] = 1

            # apply mask
            dct_coeffs_trunc = dct_coeffs * mask

            # inverse DCT
            approximated_channel = self.idct2(dct_coeffs_trunc)
            approximated[:, :, channel] = approximated_channel

        return np.clip(approximated, 0, 255).astype(np.uint8).tobytes()

