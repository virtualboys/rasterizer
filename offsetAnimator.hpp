
#ifndef offsetAnimator_hpp
#define offsetAnimator_hpp

class offsetAnimator {
public:
    offsetAnimator(float speed, float min, float max);
    
    float getVal();
    void setD(float d);
    void update();
private:
    float speed;
    float min;
    float max;
    float val;
    bool up;
};

#endif /* offsetAnimator_hpp */
