#pragma once
#include <vector>
#include <cstdint>
#include <Arduino.h> // For random() and types

struct Point {
    int x, y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

enum class Direction { UP, DOWN, LEFT, RIGHT };

class SnakeGame {
public:
    SnakeGame(); // Constructor to initialize random seed if needed
    void reset();
    void update();                    // Move snake, check collisions
    void turnLeft();                  // Counter-clockwise
    void turnRight();                 // Clockwise
    bool isGameOver() const;
    uint16_t getScore() const;

    // For rendering
    const std::vector<Point>& getSnake() const;
    Point getFood() const;

    // Constants
    static const int GRID_WIDTH = 30;
    static const int GRID_HEIGHT = 12;

private:
    std::vector<Point> snake;         // Head at index 0
    Point food;
    Direction dir;                    // Current direction
    uint16_t score;
    bool gameOver;

    void spawnFood();
    bool checkCollision(const Point& p) const;
};
