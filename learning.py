import numpy as np
import tensorflow as tf
import visualkeras
import matplotlib.pyplot as plt

from pathlib import Path

from sklearn.model_selection import train_test_split
from tinymlgen import port
from PIL import Image


# säkerställ så alla bilder är i samma format
def load_image(path, size=(224, 224)):
    img = Image.open(path).convert("RGB")
    img = img.resize(size)
    return np.array(img) / 255.0

# alla träningsdata
data_path = Path("card_project/dataset/train")

# alla olika kortklasser
classes = sorted([d.name for d in data_path.iterdir() if d.is_dir()])

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

# funktion för att extrahera färgnamn från mappnamn
def get_suit(card_name):
    return card_name.split(" of ")[-1]


# loopa genom alla klasser och ladda bilder in i olika klasserna 
for class_name in classes:
    if class_name == "joker":
        continue

    class_folder = data_path / class_name

    suit_name = get_suit(class_name)
    suit_index = suit_to_index[suit_name]

    for img_path in class_folder.glob("*.jpg"):
        arr = load_image(img_path)
        images.append(arr)
        suit_labels.append(suit_index)

# konvertera till numpy arrayer
images = np.array(images)
suit_labels = np.array(suit_labels)

# visa antal unika färgklasser och deras förekomster
print("Unique suit labels:", np.unique(suit_labels))
for i, suit in enumerate(suits):
    print(suit, "=", np.sum(suit_labels == i))


   
# skapa en mappning från etikett till klassnamn
label_to_class = {i: name for i, name in enumerate(classes)}

