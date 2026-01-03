#include "SnakeGame.h"

SnakeGame::SnakeGame() {
    reset();
}

void SnakeGame::reset() {
    snake.clear();
    // Start at center, length 3, moving RIGHT
    int startX = GRID_WIDTH / 2;
    int startY = GRID_HEIGHT / 2;
    
    snake.push_back({startX, startY});       // Head
    snake.push_back({startX - 1, startY});
    snake.push_back({startX - 2, startY});   // Tail

    dir = Direction::RIGHT;
    score = 0;
    gameOver = false;
    
    shrinkPillActive = false;
    spawnFood();
}

void SnakeGame::update() {
    if (gameOver) return;

    Point head = snake[0];
    Point newHead = head;

    switch (dir) {
        case Direction::UP:    newHead.y--; break;
        case Direction::DOWN:  newHead.y++; break;
        case Direction::LEFT:  newHead.x--; break;
        case Direction::RIGHT: newHead.x++; break;
    }

    // Check Wall Collision
    if (newHead.x < 0 || newHead.x >= GRID_WIDTH || newHead.y < 0 || newHead.y >= GRID_HEIGHT) {
        gameOver = true;
        return;
    }

    // Check Self Collision (exclude tail because it will move unless we eat food)
    // Actually, if we hit the tail segment that is about to move away, it's fine?
    // Standard snake: hitting any part of body is death.
    // If we don't eat, tail moves. If we eat, tail stays.
    // Simplest check: check against all body parts. If it matches the very last tail segment
    // AND we are not eating, technically we are chasing our tail which is valid.
    // But easier to just check all for now.
    if (checkCollision(newHead)) {
        // Special case: if newHead is exactly the tail, and we are NOT eating, it's safe.
        // But if we eat, tail grows (stays), so it would be a collision.
        // Let's keep it simple: any collision with body is bad.
        // But wait, if we don't eat, the tail pops off.
        // So checking collision against snake excluding the last element is correct IF we don't eat.
        // If we eat, we don't pop, so we check against full snake.
        // Let's see if it's food first.
    }

    // 1. Check Gold Food Timeout (10s)
    if (currentFoodType == FoodType::GOLD && millis() - foodSpawnTime > 10000) {
        spawnFood(); // Re-spawn (likely normal)
    }

    // 2. Check Shrink Pill Collision
    if (shrinkPillActive && newHead == shrinkPill) {
        shrinkPillActive = false;
        // Shrink by 5 (keep min 3)
        int removeCount = 2;
        while (snake.size() > 3 && removeCount > 0) {
            snake.pop_back();
            removeCount--;
        }
    }

    bool eating = (newHead == food);
    
    // Check self-collision
    // We iterate through the snake.
    // If we are NOT eating, the last segment will be removed, so we shouldn't collide with it.
    // If we ARE eating, the last segment stays, so we could collide with it (though impossible for head to hit tail immediately usually).
    size_t limit = snake.size();
    if (!eating) limit--; 

    for (size_t i = 0; i < limit; ++i) {
        if (newHead == snake[i]) {
            gameOver = true;
            return;
        }
    }

    // Move
    snake.insert(snake.begin(), newHead);

    if (eating) {
        if (currentFoodType == FoodType::GOLD) {
            score += 5;
        } else {
            score += 1;
        }
        
        spawnFood();
        
        // Chance to spawn shrink pill (5%)
        if (!shrinkPillActive && random(100) < 5) {
            spawnShrinkPill();
        }
    } else {
        snake.pop_back();
    }
}

void SnakeGame::turnLeft() {
    // Counter-clockwise
    switch (dir) {
        case Direction::UP:    dir = Direction::LEFT; break;
        case Direction::LEFT:  dir = Direction::DOWN; break;
        case Direction::DOWN:  dir = Direction::RIGHT; break;
        case Direction::RIGHT: dir = Direction::UP; break;
    }
}

void SnakeGame::turnRight() {
    // Clockwise
    switch (dir) {
        case Direction::UP:    dir = Direction::RIGHT; break;
        case Direction::RIGHT: dir = Direction::DOWN; break;
        case Direction::DOWN:  dir = Direction::LEFT; break;
        case Direction::LEFT:  dir = Direction::UP; break;
    }
}

bool SnakeGame::isGameOver() const {
    return gameOver;
}

uint16_t SnakeGame::getScore() const {
    return score;
}

const std::vector<Point>& SnakeGame::getSnake() const {
    return snake;
}

Point SnakeGame::getFood() const {
    return food;
}

void SnakeGame::spawnFood() {
    while (true) {
        int x = random(GRID_WIDTH);
        int y = random(GRID_HEIGHT);
        Point p = {x, y};
        // Ensure not on snake OR shrink pill
        bool collision = checkCollision(p);
        if (shrinkPillActive && p == shrinkPill) collision = true;
        
        if (!collision) {
            food = p;
            // 10% chance for Gold
            if (random(100) < 10) {
                currentFoodType = FoodType::GOLD;
            } else {
                currentFoodType = FoodType::NORMAL;
            }
            foodSpawnTime = millis();
            break;
        }
    }
}

void SnakeGame::spawnShrinkPill() {
    while (true) {
        int x = random(GRID_WIDTH);
        int y = random(GRID_HEIGHT);
        Point p = {x, y};
        // Not on snake or food
        if (!checkCollision(p) && !(p == food)) {
            shrinkPill = p;
            shrinkPillActive = true;
            shrinkPillSpawnTime = millis();
            break;
        }
    }
}

bool SnakeGame::checkCollision(const Point& p) const {
    for (const auto& s : snake) {
        if (s == p) return true;
    }
    return false;
}

SnakeGame::FoodType SnakeGame::getFoodType() const {
    return currentFoodType;
}

bool SnakeGame::isShrinkPillActive() const {
    return shrinkPillActive;
}

Point SnakeGame::getShrinkPill() const {
    return shrinkPill;
}

unsigned long SnakeGame::getCurrentSpeed() const {
    // Base 200ms, -2ms per point, cap at 80ms
    int delay = 200 - (score * 2);
    if (delay < 80) delay = 80;
    return (unsigned long)delay;
}
