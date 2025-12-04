import numpy as np
import tensorflow as tf
import visualkeras
import matplotlib.pyplot as plt

from pathlib import Path

from sklearn.model_selection import train_test_split
from tinymlgen import port
from PIL import Image

img_path = Path("../Picture_data/train/ace_of_clubs/001.jpg")
img = Image.open(img_path).convert("RGB")
arr = np.array(img)
print(arr.shape)

###def load_images_from_folder(folder: Path):
   
   