from pathlib import Path
import shutil

# Källmapp
SRC = Path("dataset")

# Målmappar
DST_CENTRAL = Path("dataset_central")
DST_PERIPHERAL = Path("dataset_peripheral")

# Vilka filtyper vi räknar som bilder
IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tif", ".tiff", ".webp"}

def is_image(p: Path) -> bool:
    return p.is_file() and p.suffix.lower() in IMAGE_EXTS

def main():
    if not SRC.exists():
        raise FileNotFoundError(f"Hittar inte mappen: {SRC.resolve()}")

    # Hämta alla bilder rekursivt och sortera stabilt
    images = sorted([p for p in SRC.rglob("*") if is_image(p)])

    if not images:
        print("Inga bilder hittades.")
        return

    # Skapa målmappar
    DST_CENTRAL.mkdir(parents=True, exist_ok=True)
    DST_PERIPHERAL.mkdir(parents=True, exist_ok=True)

    for i, src_path in enumerate(images):
        # Bevara underkatalog-strukturen (minskar risk för filkrockar)
        rel = src_path.relative_to(SRC)
        if i % 2 == 0:
            dst_path = DST_CENTRAL / rel
        else:
            dst_path = DST_PERIPHERAL / rel

        dst_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src_path, dst_path)

    print(f"Klart! {len(images)} bilder totalt.")
    print(f"Central:     {sum(1 for i in range(len(images)) if i % 2 == 0)}")
    print(f"Peripheral:  {sum(1 for i in range(len(images)) if i % 2 == 1)}")

if __name__ == "__main__":
    main()