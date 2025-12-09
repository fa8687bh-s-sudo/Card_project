import numpy as np
from pathlib import Path
from PIL import Image

# the number of for example "five of spades" images that are included in training data
TRAIN_CARDS_PER_CLASS = 8
VAL_CARDS_PER_CLASS = 2
TEST_CARDS_PER_CLASS = 2

# säkerställ så alla bilder är i samma format
def load_image(path, size=(24, 24)):
    img = Image.open(path).convert("L")
    img = img.resize(size)
    return np.array(img) / 255.0

# alla träningsdata
data_path_train = Path("dataset/train")
#alla valideringsdata
data_path_val = Path("dataset/valid")
#alla testdata
data_path_test = Path("dataset/test")

# alla olika kortklasser
classes = sorted([d.name for d in data_path_train.iterdir() if d.is_dir()])

#printa alla klasser med deras mappindex
print("Classes and their indices:")
for i, name in enumerate(classes):
    print(i, name) 

#nummer för varje färgklass från 0-3
suits = ["clubs", "diamonds", "hearts", "spades"]
suit_to_index = {s: i for i, s in enumerate(suits)}

# ladda alla bilder och deras färgklasser
suit_labels = []
images = []

# ladda alla bilder och deras färgklasser för valideringsdata
val_suit_labels = []
val_images = []

# ladda alla bilder och deras färgklasser för testdata
test_suit_labels = []
test_images = []

# funktion för att extrahera färgnamn från mappnamn
def get_suit(card_name):
    return card_name.split(" of ")[-1]

# loopa genom alla klasser och ladda valideringsbilder och labels in i olika arrayer
for class_name in classes:
    if class_name == "joker":
        continue

    class_folder = data_path_test / class_name

    suit_name = get_suit(class_name)
    suit_index = suit_to_index[suit_name]

    for i, img_path in enumerate(class_folder.glob("*.jpg")):
        if i >= TEST_CARDS_PER_CLASS:
            break
        arr = load_image(img_path)
        test_images.append(arr)
        test_suit_labels.append(suit_index)

# loopa genom alla klasser och ladda valideringsbilder och labels in i olika arrayer
for class_name in classes:
    if class_name == "joker":
        continue

    class_folder = data_path_val / class_name

    suit_name = get_suit(class_name)
    suit_index = suit_to_index[suit_name]

    for i, img_path in enumerate(class_folder.glob("*.jpg")):
        if i >= VAL_CARDS_PER_CLASS:
            break
        arr = load_image(img_path)
        val_images.append(arr)
        val_suit_labels.append(suit_index)

# loopa genom alla klasser och ladda träningsbilder och labels in i olika arrayer 
for class_name in classes:
    if class_name == "joker":
        continue

    class_folder = data_path_train / class_name
    
    suit_name = get_suit(class_name)
    suit_index = suit_to_index[suit_name]

    
    for i, img_path in enumerate(class_folder.glob("*.jpg")):
        if i >= TRAIN_CARDS_PER_CLASS:
            break
        arr = load_image(img_path)
        images.append(arr)
        suit_labels.append(suit_index)

# konvertera till numpy arrayer
images = np.array(images)
images = np.expand_dims(images, axis=-1) 
suit_labels = np.array(suit_labels)
val_images = np.array(val_images)
val_images = np.expand_dims(val_images, axis=-1)
val_suit_labels = np.array(val_suit_labels) 
test_images = np.array(test_images)
test_images = np.expand_dims(test_images, axis=-1)
test_suit_labels = np.array(test_suit_labels)

# visa antal unika färgklasser och deras förekomster
print("Unique suit labels:", np.unique(suit_labels))
for i, suit in enumerate(suits):
    print(suit, "=", np.sum(suit_labels == i))


   
# skapa en mappning från etikett till klassnamn
label_to_class = {i: name for i, name in enumerate(classes)}

# Skriv tränings och valideringsfil(alla bilder) i c++ format
def write_data_to_cpp(filename, images, labels, data_group):
    with open(filename, "a") as f:
        # f.write(f"// Data file generated from {len(images)} images\n")
        # f.write(f"#define NUM_SAMPLES {len(images)}\n")
        # f.write(f"#define IMAGE_WIDTH {images.shape[1]}\n")
        # f.write(f"#define IMAGE_HEIGHT {images.shape[2]}\n")
        # f.write(f"#define IMAGE_CHANNELS {images.shape[3]}\n\n")

        f.write("const float " + data_group + "Images[NUM_SAMPLES][IMAGE_WIDTH][IMAGE_HEIGHT][IMAGE_CHANNELS] = {\n")
        for img in images:
            f.write("  {\n")
            for row in img:
                f.write("    {")
                f.write(", ".join(f"{pixel[0]:.6f}" for pixel in row))
                f.write("},\n")
            f.write("  },\n")
        f.write("};\n\n")

        f.write("const int " + data_group + "Labels[NUM_SAMPLES] = {\n")
        for label in labels:
            f.write(f"  {label}, ")
        f.write("};\n")

# skriv träningsdata till cpp fil

def createFileAsString(images, labels, data_group):
    content = "\nconst float " + data_group + "Images["+str(images.shape[0])+"]["+str(images.shape[1])+"] = {\n" 
    for img in images:
        content += "    {"
        for pixel in img:
            content+= f"{pixel:.6f}, "
        content = content.rstrip(", ")
        content += "},\n"
    content = content.rstrip(",\n")
    content += "\n};\n"

    content += "\nconst float " + data_group + "Labels["+str(labels.shape[0])+"] = {\n    "
    for label in labels:
        content += f"{label}, "
    content = content.rstrip(", ")
    content += "\n};\n"

    return content

flat_train_images = images.reshape(images.shape[0], images.shape[1]*images.shape[2])
flat_val_images = val_images.reshape(val_images.shape[0], val_images.shape[1]*val_images.shape[2])
flat_test_images = test_images.reshape(test_images.shape[0], test_images.shape[1]*test_images.shape[2])

with open("data.h", "w") as file:
    file.write("#define NUM_TRAIN_SAMPLES " + str(flat_train_images.shape[0]) + "\n")
    file.write("#define NUM_VAL_SAMPLES " + str(flat_val_images.shape[0]) + "\n")
    file.write("#define NUM_TEST_SAMPLES " + str(flat_test_images.shape[0]) + "\n")
    file.write(createFileAsString(flat_train_images, suit_labels, "train"))
    file.write(createFileAsString(flat_val_images, val_suit_labels, "val"))
    file.write(createFileAsString(flat_test_images, test_suit_labels, "test"))


# write_data_to_cpp("data.h", flat_train_images, suit_labels, "train")

# write_data_to_cpp("data.h", flat_val_images, val_suit_labels, "val")

# write_data_to_cpp("data.h", flat_test_images, test_suit_labels, "test")

## TODO:
# Separate data for federated learning, make two separate .h files.
# Try the ML-model with the real C++ data from the data.h file
# Figure out if all images can be uploaded to Arduino, or have to be sent a few at a time
