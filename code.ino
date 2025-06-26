#include <Adafruit_NeoPixel.h>
#include <TM1637Display.h>

#define LED_PIN     6
#define WIDTH       8
#define HEIGHT      16
#define NUM_LEDS    (WIDTH * HEIGHT)

#define JOY_X       A0
#define JOY_Y       A1
#define JOY_SW      26
#define BUZZER_PIN  7

#define CLK 33  // TM1637 CLK pin
#define DIO 32  // TM1637 DIO pin

#define CLK2 42 // your NEW high score CLK
#define DIO2 43 // your NEW high score DIO

TM1637Display display(CLK, DIO);
TM1637Display highScoreDisplay(CLK2, DIO2);

Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int grid[HEIGHT][WIDTH] = {0};

unsigned long lastMove = 0;
const int moveInterval = 400;

int currentX = 3, currentY = 0;
int rotation = 0;
int currentBlockIndex = 0;
int score = 0;
int highScore = 0;


int thresholdLow = 400;
int thresholdHigh = 600;

int blockColors[] = {
  0x00FFFF, // I - Cyan
  0xFFFF00, // O - Yellow
  0x800080, // T - Purple
  0xFFA500, // L - Orange
  0x0000FF  // J - Blue
};

struct Block {
  byte shapes[4][4][4];
  int size;
};

Block blocks[] = {
  { // I
    { {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}},
      {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
      {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}} }, 4
  },
  { // O
    { {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
      {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
      {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
      {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}} }, 2
  },
  { // T
    { {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
      {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
      {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}} }, 3
  },
  { // L
    { {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
      {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
      {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}} }, 3
  },
  { // J
    { {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
      {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
      {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
      {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}} }, 3
  }
};

void setup() {
  leds.begin();
  leds.show();
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(9600);
  randomSeed(analogRead(0));
  display.setBrightness(7);
  highScoreDisplay.setBrightness(7);
  display.showNumberDec(0, true);
  highScoreDisplay.showNumberDec(0, true);
  playStartSound();
  spawnBlock();
}

void loop() {
  handleInput();
  if (millis() - lastMove > moveInterval) {
    currentY++;
    if (checkCollision()) {
      currentY--;
      mergeToGrid();
      playPlaceSound();
      clearLines();
      spawnBlock();
      if (checkCollision()) {
        playGameOverSound();
        delay(2000);
        resetGame();
        return;
      }
    }
    lastMove = millis();
  }
  draw();
  display.showNumberDec(score, true);
}

void draw() {
  leds.clear();
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if (grid[row][col]) {
        int index = getIndex(row, col);
        int color = blockColors[grid[row][col] - 1];
        leds.setPixelColor(index, color);
      }
    }
  }

  Block b = blocks[currentBlockIndex];
  for (int i = 0; i < b.size; i++) {
    for (int j = 0; j < b.size; j++) {
      if (b.shapes[rotation][i][j]) {
        int x = currentX + j;
        int y = currentY + i;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
          int index = getIndex(y, x);
          int color = blockColors[currentBlockIndex];
          leds.setPixelColor(index, color);
        }
      }
    }
  }
  leds.show();
}

void handleInput() {
  int xVal = analogRead(JOY_X);
  int yVal = analogRead(JOY_Y);
  static bool lastBtn = HIGH;
  bool btn = digitalRead(JOY_SW);

  if (xVal < thresholdLow) {
    currentX--;
    if (checkCollision()) currentX++;
    delay(150);
  } else if (xVal > thresholdHigh) {
    currentX++;
    if (checkCollision()) currentX--;
    delay(150);
  }

  if (yVal < thresholdLow) {
    currentY++;
    if (checkCollision()) currentY--;
    delay(150);
  }

  if (btn == LOW && lastBtn == HIGH) {
    rotateBlock();
  }
  lastBtn = btn;
}

void rotateBlock() {
  int prev = rotation;
  rotation = (rotation + 1) % 4;
  if (checkCollision()) rotation = prev;
}

bool checkCollision() {
  Block b = blocks[currentBlockIndex];
  for (int i = 0; i < b.size; i++) {
    for (int j = 0; j < b.size; j++) {
      if (b.shapes[rotation][i][j]) {
        int x = currentX + j;
        int y = currentY + i;
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || grid[y][x])
          return true;
      }
    }
  }
  return false;
}

void mergeToGrid() {
  Block b = blocks[currentBlockIndex];
  for (int i = 0; i < b.size; i++) {
    for (int j = 0; j < b.size; j++) {
      if (b.shapes[rotation][i][j]) {
        int x = currentX + j;
        int y = currentY + i;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
          grid[y][x] = currentBlockIndex + 1;
      }
    }
  }
}

void spawnBlock() {
  currentBlockIndex = random(0, sizeof(blocks) / sizeof(blocks[0]));
  currentX = 3;
  currentY = 0;
  rotation = 0;
}

void clearLines() {
  for (int i = HEIGHT - 1; i >= 0; i--) {
    bool full = true;
    for (int j = 0; j < WIDTH; j++) {
      if (!grid[i][j]) {
        full = false;
        break;
      }
    }
    if (full) {
      playClearLineSound();
      score += 100;
      
  display.showNumberDec(score, true);

// Check and update high score if needed
if (score > highScore) {
  highScore = score;
  highScoreDisplay.showNumberDec(highScore, true);
}

      for (int row = i; row > 0; row--) {
        for (int col = 0; col < WIDTH; col++) {
          grid[row][col] = grid[row - 1][col];
        }
      }
      for (int col = 0; col < WIDTH; col++) {
        grid[0][col] = 0;
      }
      i++; // recheck this row
    }
  }
}

void resetGame() {
  memset(grid, 0, sizeof(grid));
  score = 0;
  spawnBlock();
}

int getIndex(int row, int col) {
  return row % 2 == 0 ? row * WIDTH + col : row * WIDTH + (WIDTH - 1 - col);
}

void playStartSound() {
  tone(BUZZER_PIN, 1000, 200); delay(250);
  tone(BUZZER_PIN, 1200, 200); delay(250);
  tone(BUZZER_PIN, 1500, 300);
}

void playPlaceSound() {
  tone(BUZZER_PIN, 800, 100);
}

void playClearLineSound() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000 + i * 200, 100);
    delay(100);
  }
}

void playGameOverSound() {
  int melody[] = {400, 350, 300, 250, 200}; // Notes (frequencies)
  int noteDuration = 300; // Duration of each note

  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, melody[i], noteDuration); 
    delay(noteDuration + 50); // Delay between notes
  }

  tone(BUZZER_PIN, 500, 500); // Final note
  delay(500);

  playGameOverAnimation(); // Game over animation
}

void playGameOverAnimation() {
  unsigned long startTime = millis();
  unsigned long flashDuration = 2000; // Flash for 2 seconds
  bool flashState = true; // LED state for flashing
  int offset = 0;

  while (millis() - startTime < flashDuration) {
    if (flashState) {
      showGameOverText(offset);  // Show the text when it's on
    } else {
      leds.clear();  // Turn off LEDs when it's off
    }

    leds.show();  // Update LED grid
    flashState = !flashState;  // Toggle the state
    delay(500);  // Delay for flashing effect
    offset++;  // Move the text down
    if (offset >= HEIGHT) {
      offset = 0; // Reset the text to the top when it's done
    }
  }

  fadeOutLights();
}

void showGameOverText(int offset) {
  // Display the "Game Over" text flowing down
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if ((row - offset) >= 0 && (row - offset) <= 1) {
        if ((col == 1 || col == 2 || col == 3 || col == 4 || col == 5)) {
          leds.setPixelColor(getIndex(row, col), 255, 0, 0);  // Red color for "Game Over"
        }
      }
    }
  }
}

void fadeOutLights() {
  for (int brightness = 255; brightness >= 0; brightness--) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds.setPixelColor(i, leds.Color(0, 0, 0, brightness)); // Fade out all LEDs gradually
    }
    leds.show();
    delay(10); // Slow fade effect
  }
}
