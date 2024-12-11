Ashley Tang and Elizabeth Modano

# To use this repository:

### Data Collection:
1. Record an iPhone video of 10 people signing the ASL alphabet 
    * Notes
        * Augment this by taking 50 photos of each letter or taking videos on the microcontroller (we used the Xiao board)
        * Choose different backgrounds (a white wall, a busier background, etc)
        * Variation in skin color and hand sizes
2. Use the Google Colab notebook to turn the videos into photos
    * [Link](https://colab.research.google.com/drive/19h256NNP5GajTpGkVWqdGsOsB4ldKiND?usp=sharing)
3. Sort the photos into different folders (one folder for each sign)
Result: 5000+ images of hand signs

### Data Processing
[Example Google Colab Notebook](https://colab.research.google.com/drive/1PMYrhG6ist358anfnWGtEWE2WzI3pR8G?usp=sharing)
1. Convert dataset images to 96 x 96
2. Create training, validation, and testing folders
3. Randomize each folder of letter images
4. Create two numpy arrays for each dataset (training, validation, and testing) and use a 80/10/10 Split (80% training, 10% validation, 10% testing)
    * Create an array to store the photos of each dataset
    * Create an array to store the corresponding labels of each photo in the dataset
5. Use these two arrays to create the tensor flow datasets

### Creating the Model
1. Create a model
2. Train the model
3. Test the model accuracy using the test dataset
4. Quantize the model
5. Convert the quantized model into a .cc file

### Deploying onto Hardware
1. Replace the gmodel in the model.h file of this repo with your custom model in the .cc file you just created
    * don't forget to change the model length according to your custom model! 
2. Change the model definition in NeuralNetwork.cpp according to your custom model
3. Upload the model onto your board and view the output using the serial monitor! 