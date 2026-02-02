#pragma once

#include "ofMain.h"

class ofApp : public ofBaseApp {

public:
    void setup();
    void update();
    void draw();

    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);

    // Schedule grid: 7 days x 48 half-hour slots
    // true = ACTIVE (apps run), false = INACTIVE (darkness, apps closed)
    static const int NUM_DAYS = 7;
    static const int NUM_SLOTS = 48;  // 24 hours * 2 (30-min slots)
    bool schedule[NUM_DAYS][NUM_SLOTS];

    // App control
    vector<string> appPaths;
    vector<int> appDelays;  // Delay in seconds before launching each app
    bool appsCurrentlyRunning;
    
    // Schedule file I/O
    void loadSchedule();
    void saveSchedule();
    void loadAppList();
    
    // Time helpers
    int getCurrentDay();    // 0=Mon, 1=Tue, ... 6=Sun
    int getCurrentSlot();   // 0-47 based on current time
    string slotToTimeString(int slot);
    
    // App control
    void openApps();
    void closeApps();
    
    // Grid drawing
    void drawGrid();
    int getCellDay(int x, int y);
    int getCellSlot(int x, int y);
    
    // Grid layout
    float gridStartX;
    float gridStartY;
    float cellWidth;
    float cellHeight;
    float labelWidth;
    
    // Day names
    string dayNames[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    
    // Version
    string version;
    
    // State tracking
    int lastCheckedSlot;
    int lastCheckedDay;
    
    // Drag painting
    bool dragPaintValue;
    int lastDragDay;
    int lastDragSlot;
    
    // Test mode - override current time for testing
    bool testMode;
    int testSlot;
    int testDay;
};
