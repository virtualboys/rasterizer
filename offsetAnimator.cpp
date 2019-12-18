
#include "offsetAnimator.hpp"

offsetAnimator::offsetAnimator(float speed, float min, float max) {
    this->speed = speed;
    this->min = min;
    this->max = max;
    
    up = true;
}

void offsetAnimator::update() {
    if(up) {
        val += speed;
        if(val > max) {
            up = false;
        }
    } else {
        val -= speed;
        if(val < min) {
            up = true;
        }
    }
}

float offsetAnimator::getVal() {
    return val;
}
