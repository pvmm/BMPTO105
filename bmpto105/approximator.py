from typing import Any, Protocol


class Approximator(Protocol):
    def __init__(self, **kwargs: Any) -> None:
        # Forward all leftover kwargs up the MRO chain
        super().__init__(**kwargs)
    def approximate_tile(self, tile: bytes) -> bytes: ...

class ApproximatorCallable(Protocol):
    def __call__(self, **kwargs: int | float) -> Approximator: ...
