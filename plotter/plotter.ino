#include <SPI.h>

/// Debug
#define DEBUG
#ifdef DEBUG
  #define LOG(...)    Serial.print(__VA_ARGS__)
  #define LOGLN(...)  Serial.println(__VA_ARGS__)
#else
  #define LOG(...)    // no-op
  #define LOGLN(...)  //
#endif

/// Arduino 
const int PIN_POT = 2; // Pot
const int PIN_CS = 10; // Clock sync - MCP 4822 12-bit DAC
const int GAIN_1 = 0x1; // UNUSED (?) - HIGH - SPI Output GAIN channel
const int GAIN_2 = 0x0; // LOW - SPI Output GAIN channel
const int PIN_RELAY = 9; // Relay - Controls pen up/down

/// Coordinate / Dimensions
const float PLT_RATIO = 15.0 / 10.0;
const int MIN_DIM = 0;
const int MAX_DIM = 4096;
const int INSET = 128; // Inset increment
const float X_INSET_MULT = 8.3;  // Inset coefficientsfor calibrated center (2048, 2048)
const float Y_INSET_MULT = 12.2; //

// [-0.5, 0.5] offset params to shift dimensional offset across plate
//  0.0 | centered, no shifting
// -0.5 | inset shifted completely to MIN side
//  0.5 | inset shifted completely to MAX side
const float X_INSET_OFFSET = 0.0;  // X inset is centered on plate
const float Y_INSET_OFFSET = 0.0; // Y inset is offset completely on the top, pushing coords to bottom Y

const float LETTER_X_INSET = 4096 * (15.0-11.0) / 2.0;
const float LETTER_Y_INSET = 4096 * (10.0-8.5) / 2.0;

// Drawing
const int DELAY_SHORT = 5; // Shape drawing
void DS() { delay(DELAY_SHORT); }

const int DELAY = 20; // Linear coords
void D() { delay(DELAY); }

const int DELAY_LONG = 1000; // Step pause
void DL() { delay(DELAY_LONG); }

const int DELAY_PEN = 150; // Pen up/down
void DP() { delay(DELAY_PEN); }

boolean hasDrawn = false;

float stepSize = 0.005;

class Point {
  public:
    float x, y;

    Point(float x, float y) {
      x = x;
      y = y;
    }
  
    float dist(Point point) {
      dist(point.x, point.y);
    }
  
    float dist(float x, float y) {
      return sqrt(pow(x - x, 2) + pow(y - y, 2));
    }
};

// Current position
float currX = 0.5;
float currY = 0.5;
 
void setup() {
  Serial.begin(9600); 
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  SPI.begin();  
  SPI.setClockDivider(SPI_CLOCK_DIV2);

  penUp();
}

// MARK: - Loop
 
void loop() {
  if (!hasDrawn) {
    draw();
    hasDrawn = true;
  }
}


// Draw in me
void draw() {
  drawCircles();
//  float x0 = 0.0;
//  float y0 = 0.8;
//  setPos(x0, y0);
//
//  float len = 0.25;
//  float inc = 0.05;
//  float height = inc * 2;
//
//  float yi = y0-height;
//  
//  for (float z = 0.0; z <= len; z += inc) {
//    float xi = x0 + z;
//    
////    LOGLN(x);
////    LOGLN("\n\nLine");
////    Serial.print(xi);
////    Serial.print(",");
////    Serial.print(y0);
////    Serial.print(" -> ");
////    Serial.print(xi);
////    Serial.print(",");
////    Serial.println(yi);
//    movTo(xi, y0);
//
////    drawLine(xi, y0, xi, yi, true);
//    DL();
//  }
}

void drawCircles() {
  float x = 0.5;
    float y = 0.15;
    float rad = 0.02;
    for (float i = 0; i <= 0.1; i += 0.01) {
      float z = pow(i, 2) / 0.5;
      drawCircle(x - (z * 0.75), y, rad + z, true);
    }
}

void drawLine(float x0, float y0, float x1, float y1, boolean dryRun) {
  // Start
//  setPos(x0, y0);

  if (!dryRun) {
    // Pause and pen down
    drawWillBegin();
  }

  // End
  mov(x0, y0, x1, y1);

  if (!dryRun) {
    // Pause and pen up
    drawDidEnd();
  }
}

void drawCircle(float x,float y, float radius, boolean dryRun) {
  // Set initial coords
  double px = x + radius * cos(0);
  double py = y + radius * sin(0);
  setPos(px, py);

  if (!dryRun) {
    // Pause and pen down
    drawWillBegin();
  }

  // Draw circle
  for (float i=0; i<(360 * 1.04); i+= 0.25) { // Coeff for extra steps depending upon radius a bigger radius might need more steps
    double radians = i * PI / 180;
    double px = x + radius * cos(radians);
    double py = y + radius * sin(radians);
    setPos(px, py);
    
    delay(1); // Coord delay
  }

  if (!dryRun) {
    // Pause and pen up
    drawDidEnd();
  }
}

void drawWillBegin() {
  DP();
  penDown();
}

void drawDidEnd() {
  penUp();
  DP();
}

void diagonalLoop() {
  int stepSize = (MAX_DIM / 256);
  
  // Forward 
  for (int i = MIN_DIM; i < MAX_DIM; i += stepSize) {
    setPos(i, i);

    if (i > 1000 && i < 3000) {
      penDown();
    } else {
      penUp();
    }
    
    delay(DELAY);
  }

  // Reverse
  for (int i = MAX_DIM; i > MIN_DIM; i -= stepSize) {
    setPos(i, i);

    if (i > 1000 && i < 3000) {
      penDown();
    } else {
      penUp();
    }
    
    delay(DELAY);
  }
}

// MARK: - Pen Controls

void penUp() {
  digitalWrite(PIN_RELAY, HIGH);
}

void penDown() {
  digitalWrite(PIN_RELAY, LOW);
}


// MARK: - Scaling, Position 

// Sets the `x` and `y` axes to the given position ([0, 1], [0, 1]).
void setPos(float x, float y) {
  setXPos(x);
  setYPos(y);

  currX = x;
  currY = y;
}

// Sets the `x` axis to the given position [0, 1].
void setXPos(float x) {
  float scaledPos = scaledXPos(x);
  float inset = INSET * X_INSET_MULT;
  if (scaledPos >= inset && scaledPos <= 4096-inset) {
    setOutput(0, GAIN_2, 1, scaledPos);
  }
}

// Sets the `y` axis to the given position [0, 1].
void setYPos(float y) {
  float scaledPos = scaledYPos(y);
  float inset = INSET * X_INSET_MULT;
  if (scaledPos >= inset && scaledPos <= 4096 - inset) {
    setOutput(1, GAIN_2, 1, scaledPos);
  }
}

/// Scale from [0, 1] abstract space to the DAC's [0, 4096] space with the given inset.
float scaledXPos(float x) {
  return scaledPosition(x * (10.0 / 15.0), INSET, X_INSET_MULT, X_INSET_OFFSET);
}

float scaledYPos(float y) {
  return scaledPosition(y , INSET, Y_INSET_MULT, Y_INSET_OFFSET);
}

int scaledPosition(float pos, int inset, float insetMultiplier, float insetOffset) {
  float scaledInset = inset * insetMultiplier;
  float dimension = MAX_DIM - scaledInset;
  return (scaledInset * (0.5 + insetOffset)) + ((pos * 4096.0) / MAX_DIM) * dimension;
}


// MARK: - SPI, Utility

// channel  - `0` (x axis) or `1` (y axis)
// gain     - `GAIN_2` (low) or `GAIN_1` (high)
// shutdown - `1` (?)
// val      - value to write [0, 4096]
void setOutput(byte channel, byte gain, byte shutdown, unsigned int val)
{
  byte lowByte = val & 0xff;
  byte highByte = ((val >> 8) & 0xff) | channel << 7 | gain << 5 | shutdown << 4;
   
  PORTB &= 0xfb;
  SPI.transfer(highByte);
  SPI.transfer(lowByte);
  PORTB |= 0x4;
}

// MARK: - Plotter Drawing / Movement

void movTo(float x, float y) {
  mov(currX, currY, x, y);
}

void mov(float x0, float y0, float x1, float y1) {
  float delta = dist(x0, y0, x1, y1);
//  float delta = fmax(fabs(x1-x0), fabs(y1-y0));
  for (float i = 0.0; i < delta; i += stepSize) {
    float n = i / delta;
    float xNormalized = smoothstep(x0, x1, n);
    float yNormalized = smoothstep(y0, y1, n);
    
    float scale = 100.0;
    float xM = map(xNormalized * scale, 0, scale, x0 * scale, x1 * scale) / scale;
    float yM = map(yNormalized * scale, 0, scale, y0 * scale, y1 * scale) / scale;
    setPos(xM, yM);
    D();
    
//    Serial.print(xM);
//    Serial.print(",");
//    Serial.println(yM);
  }
}

float dist(float x0, float y0, float x1, float y1) {
  return sqrt(pow(x1 - x0, 2) + pow(y1 - y0, 2));
}


void draw(float x0, float x1) {
  float delta = fabs(x1 - x0);
  for (float i = 0.0; i < delta; i += stepSize) {
    float n = i / delta;
    float x = smoothstep(x0, x1, n);
    Serial.println(x);
  }
}

float smoothstep(float x0, float x1, float val) {
  // Scale, bias and saturate x to 0..1 range
  val= clamp((val - x0) / (x1 - x0), 0.0, 1.0); 
  // Evaluate polynomial
  return val * val * (3 - 2 * val);
}

float clamp(float x, float lowerlimit, float upperlimit) {
  if (x < lowerlimit)
    x = lowerlimit;
  if (x > upperlimit)
    x = upperlimit;
  return x;
}
