from typing import Any, Protocol


class Approximator(Protocol):

    def __init__(self, **kwargs: Any) -> None:
        # Forward all leftover kwargs up the MRO chain
        super().__init__(**kwargs)

    def approximate_tile(self, tile: bytes) -> bytes: ...


class NulApproximator(Approximator):
    def approximate_tile(self, tile: bytes) -> bytes:
        '''nul approximator'''
        return tile


class ApproximatorCallable(Protocol):
    def __call__(self, **kwargs: int | float) -> Approximator: ...
