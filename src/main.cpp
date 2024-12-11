
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "NeuralNetwork.h"

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#include "camera_pins.h"

#define INPUT_W 96
#define INPUT_H 96
#define LED_BUILT_IN 21

#define DEBUG_TFLITE 0

int count;

#if DEBUG_TFLITE==1
#include "img.h"  // Use a static image for debugging
float img_data[INPUT_W * INPUT_H * 3] = {0};
#endif

NeuralNetwork *g_nn;


// Convert RGB565 to RGB888
uint32_t rgb565torgb888(uint16_t color)
{
    uint8_t hb, lb;
    uint32_t r, g, b;

    lb = (color >> 8) & 0xFF;
    hb = color & 0xFF;

    r = (lb & 0x1F) << 3;
    g = ((hb & 0x07) << 5) | ((lb & 0xE0) >> 3);
    b = (hb & 0xF8);

    return (r << 16) | (g << 8) | b;
}


const char labels[26] = {'K','W','O','Z','U','F','H','V','A','T','D','Q','X','S','G','L','N','M','C','B','I','J','E','R','P','Y'};
//const char labels[26] = {'A','L','U','A', 'L','U','A','L','U','A','L','U','A','L','U','A','L','U','A','L','U','A','L','U','A','L'};
// switched const char labels[26] = {'K','W','O','Z','U','F','H','V','A','T','I','Q','X','S','G','L','N','M','C','B','D','J','E','R','P','Y'};
//const char labels[26] = {'Q','F','X','J','U','H','M','V','S','A','E','R','W','N','Y','G','B','Z','K','T','I','C','D','O','P','L'};
const int label_count = sizeof(labels);


/*
// Get image from camera frame buffer and convert it to the input tensor
int GetImage(camera_fb_t * fb, TfLiteTensor* input) 
{
    assert(fb->format == PIXFORMAT_RGB565);

    // Trimming Image
    int post = 0;
    int startx = (fb->width - INPUT_W) / 2;
    int starty = (fb->height - INPUT_H);
    for (int y = 0; y < INPUT_H; y++) {
        for (int x = 0; x < INPUT_W; x++) {
            int getPos = (starty + y) * fb->width + startx + x;
            //MicroPrintf("input[%d]: fb->buf[%d]=%d\n", post, getPos, fb->buf[getPos]);
            uint16_t color = ((uint16_t *)fb->buf)[getPos];
            uint32_t rgb = rgb565torgb888(color);

            float *image_data = input->data.f;

            image_data[post * 3 + 0] = ((rgb >> 16) & 0xFF)/255.0f;  // R
            image_data[post * 3 + 1] = ((rgb >> 8) & 0xFF)/255.0f;   // G
            image_data[post * 3 + 2] = (rgb & 0xFF)/255.0f;          // B
            //image_data[post] = grayscale / 255.0f; // Normalize to [0, 1]
            post++;
            //Serial.print(image_data[post * 3 + 0]);
            //Serial.print(image_data[post * 3 + 1]);
            //Serial.print(image_data[post * 3 + 2]);
            //Serial.print(" ");
        }
        //Serial.println();
    }
    return 0;
}
*/

int GetImage(camera_fb_t * fb, TfLiteTensor* input) 
{
    assert(fb->format == PIXFORMAT_RGB565);

    // Quantization parameters
    float scale = 0.003921568859368563;
    int zero_point = -128;

    // Initialize variables
    int post = 0;
    int startx = (fb->width - INPUT_W) / 2;
    int starty = (fb->height - INPUT_H);

    for (int y = 0; y < INPUT_H; y++) {
        for (int x = 0; x < INPUT_W; x++) {
            int getPos = (starty + y) * fb->width + startx + x;
            uint16_t color = ((uint16_t *)fb->buf)[getPos];

            uint8_t rgb = rgb565torgb888(color);

            int8_t *image_data = input->data.int8;
            // Quantize and store in the input tensor
            image_data[post * 3 + 0] = round(((rgb >> 16) & 0xFF)/255.0f/scale) + zero_point;  // R
            image_data[post * 3 + 1] = round(((rgb >> 8) & 0xFF)/255.0f/scale) + zero_point;   // G
            image_data[post * 3 + 2] = round((rgb & 0xFF)/255.0f/scale) + zero_point;          // B

            post++;
        }
    }
    //Serial.println("image loaded!");
    return 0;
}



/*
void PopulateImageData(camera_fb_t* fb) {
    assert(fb->format == PIXFORMAT_RGB565);

    int post = 0;
    int startx = (fb->width - INPUT_W) / 2; // Center crop width
    int starty = (fb->height - INPUT_H);   // Bottom crop height

    for (int y = 0; y < INPUT_H; y++) {
        for (int x = 0; x < INPUT_W; x++) {
            int getPos = (starty + y) * fb->width + startx + x;

            uint16_t color = ((uint16_t*)fb->buf)[getPos]; // Get pixel in RGB565 format
            uint32_t rgb = rgb565torgb888(color);          // Convert to RGB888

            img_data[post * 3 + 0] = (rgb >> 16) & 0xFF; // R
            img_data[post * 3 + 1] = (rgb >> 8) & 0xFF;  // G
            img_data[post * 3 + 2] = rgb & 0xFF;         // B

            post++;
        }
    }

}
*/


void setup() {
  count = 0;
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);

  while(!Serial) {
    static int retries = 0;
    delay(1000); // Wait for serial monitor to open
    if (retries++ > 5) {
      break;
    }
  } // When the serial monitor is turned on, the program starts to execute
  
  Serial.setDebugOutput(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_96X96;
  config.pixel_format = PIXFORMAT_RGB565; // PIXFORMAT_JPEG; // for streaming
  //config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 3;
  config.fb_count = 1;
  
  // Pin for LED
  pinMode(LED_BUILT_IN, OUTPUT); // Set the pin as output

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  Serial.printf("Camera init success!\n");
  Serial.printf("frame_size=%d\n", config.frame_size);
  Serial.printf("pixel_format=%d\n", config.pixel_format);

  // Initialize neural network
  Serial.println("Initializing neural network...");
  g_nn = new NeuralNetwork();

}

// Main loop
void loop() {

  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    res = ESP_FAIL;
  } else {
    if(fb->format != PIXFORMAT_JPEG){
      uint64_t start, dur_prep, dur_infer;
#if DEBUG_TFLITE==0
      // Use camera image
      start = esp_timer_get_time();
      //GetImage(fb, g_nn->getInput());
      GetImage(fb, g_nn->getInput());
      dur_prep = esp_timer_get_time() - start;
#else
      // Use a static image for debugging
      PopulateImageData(fb);
      memcpy(g_nn->getInput()->data.f, img_data, sizeof(img_data));
      printf("input: %.3f %.3f %.3f...\n", 
        g_nn->getInput()->data.f[0], g_nn->getInput()->data.f[1], g_nn->getInput()->data.f[2]);
#endif
      start = esp_timer_get_time();
      g_nn->predict();
      dur_infer = esp_timer_get_time() - start;
      //ing: %llu ms, Inference: %llu ms\n", dur_prep/1000, dur_infer/1000);

      /*
      //SOFTMAX by hand
      for (int i = 0; i < label_count; i++) {
        probabilities[i] = exp((g_nn->getOutput()->data.f64[1][i]));
      }
      float sum = 0.0f;
      for (int i = 0; i < label_count; i++) {
          sum += g_nn->getOutput()->data.f64[i];
      }
      for (int i = 0; i < label_count; i++) {
          probabilities[i] /= sum;
      }
      */
      
      float probabilities[26];
      float scale = 0.00390625;
      int zero_point = -128;
      float prob = 0;
      for(int i = 0; i < 26; i++){
        //MicroPrintf("output[%d]: tensor[%d]=%d\n", i, i, g_nn->getOutput()->data.int8[i]);
        float quantized_tensor = g_nn->getOutput()->data.int8[i];
        float dequantized_tensor = scale * (quantized_tensor - zero_point);
        //Serial.printf("Dequantized Tensor: %f", dequantized_tensor);
        probabilities[i] = dequantized_tensor;
        //Serial.print(probabilities[i]);
        //Serial.println(labels[i]);

      }
      int max_label = 0;
      for(int i = 0; i < 26; i++){
        //float current_prob = probabilities[i];
        float current_prob =  probabilities[i];
        //Serial.print(labels[i]);
        //Serial.println(current_prob);
        if((current_prob > prob)){
          prob = current_prob;
          max_label = i;
        }
        //Serial.printf("Letter: %s Probability: %f", labels[i], g_nn->getOutput()->data.f[i]);
      }
      char letter = labels[max_label];
      if(count % 10 == 0){
        Serial.printf("output: %f --> ", prob);
        if (prob < 0.25) {
          Serial.println("Unknown");
        //digitalWrite(LED_BUILT_IN, HIGH); //Turn off
        } else {
          Serial.println(letter);
        // show the inference result on LED
        //digitalWrite(LED_BUILT_IN, LOW); //Turn on
        }
      }
      //Serial.print(count);
      count += 1;
      esp_camera_fb_return(fb);
      fb = NULL;
      //delay(1000);
    }
  }
}