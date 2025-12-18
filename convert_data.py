import numpy as np
import matplotlib.pyplot as plt
import random
from pathlib import Path
from PIL import Image, ImageOps

CLASSES = ("diamonds", "clubs", "hearts", "spades")
RESOLUTION = 128 # Images get resized to RESOLUTION x RESOLUTION pixels
WHITE_THRESHOLD = 170 # The grayscale pixel value needed to qualify as white
TRAINING_RATIO = 0.8 # The ratio of the data that is used for training (the rest is used for validation)

def file_to_bits(image):
    image = ImageOps.exif_transpose(image) # Photos taken with phone contain information about if they need to be rotated
    image = image.convert("L").resize((RESOLUTION, RESOLUTION)) # Grayscale and desired resolution
    image_array = np.asarray(image)
    #return image_array / 255.0
    return (image_array > WHITE_THRESHOLD).astype(np.uint8)


def visualise_images(images):
    if not isinstance(images, (list, tuple)):
        images = [images]

    for image in images:
        image = np.array(image, dtype=np.uint8)
        bits = np.unpackbits(image)[:RESOLUTION * RESOLUTION]
        image = bits.reshape((RESOLUTION, RESOLUTION))
        
        plt.figure()
        plt.imshow(image, cmap='gray', vmin=0, vmax=1)
        plt.axis('off')
    
    plt.show()

def bits_to_bytes(array):
    bytes_array = []
    for row in array:
        byte = 0
        bits_added = 0
        for i, bit in enumerate(row):
            byte = (byte << 1) | bit
            bits_added += 1
            if (i + 1) % 8 == 0:
                bytes_array.append(byte)
                byte = 0
                bits_added = 0
        if bits_added > 0:
            byte = byte << (8 - bits_added) # Shift the leftover bits so they are padded with 0 to the right instead of left
            bytes_array.append(byte)
    return bytes_array

def load_images(path):
    images = []
    labels = []
    for label, class_name in enumerate(CLASSES):
        for file_path in path.joinpath(class_name).glob("*.jpeg"):
            if not file_path.stem[0].isdigit():  # For now we only look at the number cards
                continue
            image = Image.open(file_path)
            image_array = file_to_bits(image)
            packed_image = bits_to_bytes(image_array)
            images.append(packed_image)
            labels.append(label)
    return np.asarray(images), np.asarray(labels)

def train_val_split(images, labels):
    images_and_labels = list(zip(images, labels))
    random.shuffle(images_and_labels)
    images, labels = zip(*images_and_labels)
    images = np.asarray(images)
    labels = np.asarray(labels)

    first_val_index = int(TRAINING_RATIO * images.shape[0])
    train_images = images[:first_val_index]
    train_labels = labels[:first_val_index]
    val_images = images[first_val_index:]
    val_labels = labels[first_val_index:]

    return train_images, train_labels, val_images, val_labels

def central_peripheral_split(images, labels):
    first_peripheral_index = images.shape[0] // 2
    central_images = images[:first_peripheral_index]
    central_labels = labels[:first_peripheral_index]
    peripheral_images = images[first_peripheral_index:]
    peripheral_labels = labels[first_peripheral_index:]
    return central_images, central_labels, peripheral_images, peripheral_labels


def create_file_content(images, labels, device, data_group):
    content = f"\nconst uint8_t {device}{data_group}Images[{images.shape[0]}][{images.shape[1]}] PROGMEM = {{\n"
    for image in images:
        content += "    {"
        content += ", ".join(str(byte) for byte in image)
        content += "},\n"
    content += "};\n"

    content += f"\nconst uint8_t {device}{data_group}Labels[{labels.shape[0]}] PROGMEM = {{\n    "
    content += ", ".join(str(l) for l in labels)
    content += "\n};\n"

    return content

def write_to_file(file_path, content_tuple):
    with open(file_path, "w") as file:
        for content in content_tuple:
            file.write(content)


images, labels = load_images(Path("dataset"))
train_images, train_labels, val_images, val_labels = train_val_split(images, labels)
central_train_images, central_train_labels, peripheral_train_images, peripheral_train_labels = central_peripheral_split(train_images, train_labels)
central_val_images, central_val_labels, peripheral_val_images, peripheral_val_labels = central_peripheral_split(val_images, val_labels)

central_train_data_string = create_file_content(central_train_images, central_train_labels, "central", "Train")
central_val_data_string = create_file_content(central_val_images, central_val_labels, "central", "Val")
peripheral_train_data_string = create_file_content(peripheral_train_images, peripheral_train_labels, "peripheral", "Train")
peripheral_val_data_string = create_file_content(peripheral_val_images, peripheral_val_labels, "peripheral", "Val")
inclusions = "#pragma once\n#include <cstdint>\n#include <Arduino.h>\n"
definitions = f"\n#define NBR_CENTRAL_TRAIN_IMAGES {central_train_images.shape[0]}\n#define NBR_CENTRAL_VAL_IMAGES {central_val_images.shape[0]}\n"
definitions += f"#define NBR_PERIPHERAL_TRAIN_IMAGES {peripheral_train_images.shape[0]}\n#define NBR_PERIPHERAL_VAL_IMAGES {peripheral_val_images.shape[0]}\n"
write_to_file("model/Data.h", (inclusions, definitions, central_train_data_string, central_val_data_string, peripheral_train_data_string, peripheral_val_data_string))

# Added to get a test dataset if the camera doesn't work
test_images, test_labels = load_images(Path("test_data"))
test_data_string = create_file_content(test_images, test_labels, "", "test")
test_definition = f"\n#define NBR_TEST_IMAGES {test_images.shape[0]}\n"
write_to_file("model/TestData.h", (inclusions, test_definition, test_data_string))