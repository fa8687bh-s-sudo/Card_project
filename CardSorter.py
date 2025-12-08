from pathlib import Path
from PIL import Image
import numpy as np
import random

# === PARAMETRAR ===
# Mapp där alla bilder ligger (t.ex. "dataset")
IMAGE_FOLDER = Path("dataset")

# Storlek på bilden som modellen ska använda (IMG_SIZE x IMG_SIZE)
IMG_SIZE = 24  # ändra till 32 om ni vill

# Filnamn på header-filen som ska genereras
OUTPUT_HEADER = "card_dataset.h"

# Suit-map: sista bokstaven i filnamnet (utan ändelse) -> label
# Just nu: suits (4 klasser)
SUIT_MAP = {
    "H": 0,   # Hearts
    "D": 1,   # Diamonds
    "C": 2,   # Clubs
    "S": 3,   # Spades
}

def load_images_from_folder(folder: Path):
    X = []
    Y = []

    if not folder.exists():
        raise FileNotFoundError(f"Mappen {folder} finns inte")

    # Ta alla rimliga bildtyper
    image_paths = []
    for ext in ("*.jpg", "*.jpeg", "*.png", "*.bmp"):
        image_paths.extend(folder.glob(ext))

    if not image_paths:
        print(f"Inga bilder hittades i {folder}")
        return np.array([]), np.array([])

    print(f"Hittade {len(image_paths)} bilder i {folder}")

    for path in image_paths:
        stem = path.stem  # t.ex. "2D", "10S", "KH"

        if len(stem) < 2:
            print(f"Hoppar över (för kortt filnamn): {path.name}")
            continue

        suit_letter = stem[-1].upper()  # sista tecknet
        if suit_letter not in SUIT_MAP:
            print(f"Hoppar över (okänd suit '{suit_letter}'): {path.name}")
            continue

        label = SUIT_MAP[suit_letter]

        try:
            img = Image.open(path).convert("L")  # gråskala
        except Exception as e:
            print(f"Kunde inte läsa {path.name}: {e}")
            continue

        img = img.resize((IMG_SIZE, IMG_SIZE))
        arr = np.array(img, dtype=np.float32)

        # Normalisera 0–1
        arr = arr / 255.0

        X.append(arr.flatten())
        Y.append(label)

    X = np.array(X, dtype=np.float32)
    Y = np.array(Y, dtype=np.uint8)

    print(f"Totalt: {X.shape[0]} bilder, {X.shape[1]} features per bild")
    return X, Y

def shuffle_together(X: np.ndarray, Y: np.ndarray):
    indices = list(range(len(X)))
    random.shuffle(indices)
    X = X[indices]
    Y = Y[indices]
    return X, Y

def write_header(X: np.ndarray, Y: np.ndarray, filename: str):
    n_samples, input_dim = X.shape
    n_classes = len(set(Y.tolist()))

    print(f"Skriver header med {n_samples} samples, "
          f"input_dim={input_dim}, n_classes={n_classes}")

    with open(filename, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define N_SAMPLES {n_samples}\n")
        f.write(f"#define INPUT_DIM {input_dim}\n")
        f.write(f"#define N_CLASSES {n_classes}\n\n")

        # Kommentera vilken label som motsvarar vilken suit
        f.write("// Label-mappning (suits):\n")
        inv_suit_map = {v: k for k, v in SUIT_MAP.items()}
        for label in sorted(inv_suit_map.keys()):
            suit_char = inv_suit_map[label]
            name = {
                "H": "hearts",
                "D": "diamonds",
                "C": "clubs",
                "S": "spades",
            }.get(suit_char, "?")
            f.write(f"//   {label} = {name} ({suit_char})\n")
        f.write("\n")

        # Skriv X
        f.write(f"const float train_X[N_SAMPLES][INPUT_DIM] = {{\n")
        for i in range(n_samples):
            row = X[i]
            vals = ", ".join(f"{v:.6f}f" for v in row)
            f.write(f"  {{{vals}}},\n")
        f.write("};\n\n")

        # Skriv y
        f.write("const uint8_t train_y[N_SAMPLES] = {\n  ")
        f.write(", ".join(str(int(v)) for v in Y))
        f.write("\n};\n")

    print(f"Klart! Skrev {filename}")

def main():
    random.seed(42)

    X, Y = load_images_from_folder(IMAGE_FOLDER)
    if X.size == 0:
        print("Inga giltiga bilder att skriva.")
        return

    X, Y = shuffle_together(X, Y)
    write_header(X, Y, OUTPUT_HEADER)

if __name__ == "__main__":
    main()
