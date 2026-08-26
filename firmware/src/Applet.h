#ifndef APPLET_H
#define APPLET_H

class Applet {
public:
    virtual ~Applet() = default;

    // Lifecycle methods
    virtual void init() = 0;
    virtual void update() = 0;
    virtual void draw() = 0;
    virtual void cleanup() {}
    virtual void onPrepareSleep() {}

    // Input event handlers
    virtual void onActionClick() {}
    virtual void onActionHold() {}
    virtual void onModeClick() {}
    virtual void onModeHold() {}
    virtual void onBothHeld() {}
};

#endif // APPLET_H
