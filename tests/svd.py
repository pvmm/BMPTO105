import numpy as np
from PIL import Image
import hashlib
import os
import argparse
from collections import defaultdict
from typing import List, Tuple, Dict
import struct

class TileProcessor:
    def __init__(self, threshold: float = 0.1):
        """
        Inicializa o processador de tiles
        
        Args:
            threshold: Limiar para truncamento dos valores singulares (0-1)
                      Quanto maior, mais agressiva a compressão
        """
        self.threshold = threshold
        
    def tile_to_matrix(self, tile: bytes) -> np.ndarray:
        """
        Converte um tile (64 bytes RGB) em uma matriz 8x8x3
        """
        # Converte bytes para array de uint8
        pixels = np.frombuffer(tile, dtype=np.uint8)
        # Reshape para 8x8x3
        return pixels.reshape(8, 8, 3)
    
    def matrix_to_tile(self, matrix: np.ndarray) -> bytes:
        """
        Converte uma matriz 8x8x3 de volta para bytes
        """
        # Garante que os valores estão no intervalo [0, 255]
        matrix = np.clip(matrix, 0, 255).astype(np.uint8)
        # Converte para bytes
        return matrix.tobytes()
    
    def approximate_tile(self, tile: bytes) -> bytes:
        """
        Aplica aproximação de baixo rank usando SVD truncado
        """
        # Converte tile para matriz
        matrix = self.tile_to_matrix(tile)
        
        # Processa cada canal RGB separadamente
        approximated = np.zeros_like(matrix, dtype=np.float32)
        
        for channel in range(3):
            # Extrai o canal
            channel_matrix = matrix[:, :, channel].astype(np.float32)
            
            # Aplica SVD
            U, s, Vt = np.linalg.svd(channel_matrix, full_matrices=False)
            
            # Calcula número de valores singulares a manter baseado no threshold
            s_total = np.sum(s)
            if s_total > 0:
                # Encontra quantos valores singulares são necessários para manter
                # a proporção especificada da energia total
                s_cumsum = np.cumsum(s) / s_total
                k = np.searchsorted(s_cumsum, 1.0 - self.threshold) + 1
                k = max(1, min(k, len(s)))  # Mantém pelo menos 1 e no máximo todos
            else:
                k = 1
            
            # Trunca os valores singulares
            s_truncated = np.zeros_like(s)
            s_truncated[:k] = s[:k]
            
            # Reconstrói a matriz com rank reduzido
            approximated_channel = U @ np.diag(s_truncated) @ Vt
            
            # Armazena o canal aproximado
            approximated[:, :, channel] = approximated_channel
        
        # Converte de volta para tile
        return self.matrix_to_tile(approximated)
    
    def compute_tile_hash(self, tile: bytes) -> str:
        """
        Calcula o MD5 hash de um tile
        """
        return hashlib.md5(tile).hexdigest()
    
    def process_tile_list(self, tiles: List[bytes]) -> Dict[str, int]:
        """
        Processa uma lista de tiles, removendo duplicatas aproximadas
        
        Args:
            tiles: Lista de tiles (cada um como bytes de 64 valores RGB)
            
        Returns:
            Dicionário com hash -> contagem de tiles únicos
        """
        # Dicionário para armazenar tiles únicos
        unique_tiles = {}
        tile_counts = defaultdict(int)
        
        for i, tile in enumerate(tiles):
            # Aplica aproximação
            approximated_tile = self.approximate_tile(tile)
            
            # Calcula hash do tile aproximado
            tile_hash = self.compute_tile_hash(approximated_tile)
            
            # Armazena o tile original e sua contagem
            if tile_hash not in unique_tiles:
                unique_tiles[tile_hash] = tile
            tile_counts[tile_hash] += 1
        
        return dict(tile_counts)
    
    def save_unique_tiles(self, unique_tiles: Dict[str, bytes], output_dir: str):
        """
        Salva os tiles únicos como imagens individuais
        """
        os.makedirs(output_dir, exist_ok=True)
        
        for i, (hash_val, tile_data) in enumerate(unique_tiles.items()):
            # Converte bytes para imagem 8x8
            pixels = np.frombuffer(tile_data, dtype=np.uint8).reshape(8, 8, 3)
            img = Image.fromarray(pixels, 'RGB')
            img = img.resize((64, 64), Image.NEAREST)  # Amplia para visualização
            
            # Salva imagem
            filename = f"tile_{i:04d}_{hash_val[:8]}.png"
            img.save(os.path.join(output_dir, filename))


class ImageTileExtractor:
    @staticmethod
    def extract_tiles_from_image(image_path: str, tile_size: int = 8) -> List[bytes]:
        """
        Extrai tiles de uma imagem
        """
        img = Image.open(image_path)
        img = img.convert('RGB').crop((0, 64, 256, 128))
        width, height = img.size
        
        tiles = []
        pixels = np.array(img)
        
        for y in range(0, height, tile_size):
            for x in range(0, width, tile_size):
                if y + tile_size <= height and x + tile_size <= width:
                    tile = pixels[y:y+tile_size, x:x+tile_size, :]
                    # Converte para bytes
                    tile_bytes = tile.astype(np.uint8).tobytes()
                    tiles.append(tile_bytes)
        
        return tiles


def main():
    parser = argparse.ArgumentParser(description='Remove tiles semelhantes usando SVD')
    parser.add_argument('image1', help='Caminho para a imagem de entrada 1')
    parser.add_argument('image2', help='Caminho para a imagem de entrada 2')
    parser.add_argument('--threshold', type=float, default=0.1,
                       help='Limiar para truncamento SVD (0-1). Quanto maior, mais agressiva a compressão')
    parser.add_argument('--output', default='unique_tiles',
                       help='Diretório para salvar os tiles únicos')
    parser.add_argument('--tile-size', type=int, default=8,
                       help='Tamanho do tile (padrão: 8)')
    
    args = parser.parse_args()
    
    # Extrai tiles da imagem
    extractor = ImageTileExtractor()
    print(f"Extraindo tiles da imagem '{args.image1}'...")
    tiles = extractor.extract_tiles_from_image(args.image1, args.tile_size)
    print(f"Extraindo tiles da imagem '{args.image2}'...")
    tiles += extractor.extract_tiles_from_image(args.image2, args.tile_size)

    print(f"Total de tiles extraídos: {len(tiles)}")
    
    # Processa tiles
    processor = TileProcessor(threshold=args.threshold)
    print(f"Processando tiles com threshold={args.threshold}...")
    unique_tile_counts = processor.process_tile_list(tiles)
    
    print(f"Tiles únicos encontrados: {len(unique_tile_counts)}")
    print(f"Redução: {len(tiles) - len(unique_tile_counts)} tiles removidos")
    
    # Mostra estatísticas
    print("\nEstatísticas de frequência dos tiles:")
    sorted_counts = sorted(unique_tile_counts.items(), key=lambda x: -x[1])
    for i, (hash_val, count) in enumerate(sorted_counts[:10]):
        print(f"  Tile {i+1}: {count} ocorrências (hash: {hash_val[:8]}...)")
    
    # Salva tiles únicos
    # Precisamos recuperar os tiles únicos do processador
    # O processador não retorna os tiles originais, apenas as contagens
    # Vamos reprocessar para salvar os tiles únicos
    print(f"\nSalvando tiles únicos em '{args.output}'...")
    
    # Reprocessa para obter os tiles únicos
    unique_tiles_dict = {}
    tile_counts = defaultdict(int)
    
    for tile in tiles:
        approximated_tile = processor.approximate_tile(tile)
        tile_hash = processor.compute_tile_hash(approximated_tile)
        if tile_hash not in unique_tiles_dict:
            unique_tiles_dict[tile_hash] = tile  # Guarda o tile original
        tile_counts[tile_hash] += 1
    
    processor.save_unique_tiles(unique_tiles_dict, args.output)
    print("Concluído!")


# Exemplo de uso programático
def example_usage():
    """
    Exemplo de como usar a classe TileProcessor programaticamente
    """
    # Cria alguns tiles de exemplo (8x8 RGB)
    import random
    
    # Cria 10 tiles aleatórios
    tiles = []
    for _ in range(10):
        # Cria um tile 8x8 com valores RGB aleatórios
        tile_data = bytes([random.randint(0, 255) for _ in range(8 * 8 * 3)])
        tiles.append(tile_data)
    
    # Adiciona algumas variações do mesmo tile
    base_tile = tiles[0]
    for i in range(3):
        # Cria uma cópia com pequenas variações
        variation = bytearray(base_tile)
        for j in range(10):  # Muda alguns pixels
            pos = random.randint(0, len(variation)-1)
            variation[pos] = (variation[pos] + random.randint(-10, 10)) % 256
        tiles.append(bytes(variation))
    
    print(f"Total de tiles: {len(tiles)}")
    
    # Processa com diferentes thresholds
    for threshold in [0.0, 0.1, 0.3, 0.5]:
        processor = TileProcessor(threshold=threshold)
        unique_counts = processor.process_tile_list(tiles)
        print(f"Threshold {threshold:.1f}: {len(unique_counts)} tiles únicos")
    
    # Salva os tiles únicos
    processor = TileProcessor(threshold=0.1)
    unique_tiles_dict = {}
    for tile in tiles:
        approximated_tile = processor.approximate_tile(tile)
        tile_hash = processor.compute_tile_hash(approximated_tile)
        if tile_hash not in unique_tiles_dict:
            unique_tiles_dict[tile_hash] = tile
    
    processor.save_unique_tiles(unique_tiles_dict, "example_output")


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        main()
    else:
        print("Executando exemplo...")
        example_usage()
