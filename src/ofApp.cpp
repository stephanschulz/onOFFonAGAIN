#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    version = "scheduleDarkness v1";
    ofLog() << "Starting " << version;
    
    // Grid layout: ASCII style - days as columns, time slots as rows
    // Using bitmap font: 8 pixels per character
    labelWidth = 6 * 8;   // "00:00 " = 6 chars
    gridStartX = 5 + labelWidth + 8;  // After time label + " "
    gridStartY = 70;      // More space for header
    cellWidth = 5 * 8;    // " [#] " = 5 chars = 40 pixels (centered with spaces)
    cellHeight = 17;      // Line height + 3 extra pixels spacing
    
    // Set window size (1.5x wider)
    int windowWidth = (5 + labelWidth + 8 + (NUM_DAYS * cellWidth) + 100) * 1.5;
    int windowHeight = gridStartY + 12 + (NUM_SLOTS * cellHeight) + 150;  // Extra for multi-line status + app list
    ofSetWindowShape(windowWidth, windowHeight);
    ofSetWindowTitle(version);
    
    // Initialize schedule to all inactive (apps closed by default)
    for (int d = 0; d < NUM_DAYS; d++) {
        for (int s = 0; s < NUM_SLOTS; s++) {
            schedule[d][s] = false;
        }
    }
    
    // Load saved schedule if exists
    loadSchedule();
    
    // Load app list
    loadAppList();
    
    // Initialize state
    appsCurrentlyRunning = true;  // Assume apps are running at start
    lastCheckedSlot = -1;
    lastCheckedDay = -1;
    
    ofSetFrameRate(60);  // Smooth UI responsiveness
}

//--------------------------------------------------------------
void ofApp::loadSchedule() {
    string path = ofToDataPath("schedule.json");
    ofFile file(path);
    
    if (file.exists()) {
        ofJson json;
        file >> json;
        
        if (json.contains("schedule")) {
            auto& sched = json["schedule"];
            for (int d = 0; d < NUM_DAYS && d < sched.size(); d++) {
                for (int s = 0; s < NUM_SLOTS && s < sched[d].size(); s++) {
                    schedule[d][s] = sched[d][s].get<bool>();
                }
            }
            ofLog() << "Loaded schedule from " << path;
        }
    } else {
        ofLog() << "No schedule.json found, using defaults (all active)";
        saveSchedule();  // Create default file
    }
}

//--------------------------------------------------------------
void ofApp::saveSchedule() {
    ofJson json;
    
    for (int d = 0; d < NUM_DAYS; d++) {
        ofJson dayArray;
        for (int s = 0; s < NUM_SLOTS; s++) {
            dayArray.push_back(schedule[d][s]);
        }
        json["schedule"].push_back(dayArray);
    }
    
    string path = ofToDataPath("schedule.json");
    ofFile file(path, ofFile::WriteOnly);
    file << json.dump(2);
    file.close();
    
    ofLog() << "Saved schedule to " << path;
}

//--------------------------------------------------------------
void ofApp::loadAppList() {
    appPaths.clear();
    
    string path = ofToDataPath("appsToControl.txt");
    ofFile file(path);
    
    if (file.exists()) {
        ofBuffer buffer = file.readToBuffer();
        
        for (auto& line : buffer.getLines()) {
            string trimmed = line;
            // Trim whitespace
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
            
            if (trimmed.length() > 0 && trimmed[0] != '#') {
                appPaths.push_back(trimmed);
                ofLog() << "App to control: " << trimmed;
            }
        }
        ofLog() << "Loaded " << appPaths.size() << " apps from " << path;
    } else {
        ofLogWarning() << "No appsToControl.txt found at " << path;
    }
}

//--------------------------------------------------------------
int ofApp::getCurrentDay() {
    // Get current day of week (0=Mon, 6=Sun)
    time_t now = time(0);
    tm* ltm = localtime(&now);
    int dow = ltm->tm_wday;  // 0=Sunday in C
    // Convert to Mon=0, Sun=6
    return (dow == 0) ? 6 : dow - 1;
}

//--------------------------------------------------------------
int ofApp::getCurrentSlot() {
    // Get current 30-min slot (0-47)
    time_t now = time(0);
    tm* ltm = localtime(&now);
    int hour = ltm->tm_hour;
    int minute = ltm->tm_min;
    return hour * 2 + (minute >= 30 ? 1 : 0);
}

//--------------------------------------------------------------
string ofApp::slotToTimeString(int slot) {
    int hour = slot / 2;
    int minute = (slot % 2) * 30;
    char buffer[6];
    sprintf(buffer, "%02d:%02d", hour, minute);
    return string(buffer);
}

//--------------------------------------------------------------
void ofApp::openApps() {
    if (appPaths.empty()) return;
    
    ofLog() << "Opening apps...";
    for (auto& appPath : appPaths) {
        string command = "open \"" + appPath + "\"";
        ofLog() << "  " << command;
        system(command.c_str());
    }
    appsCurrentlyRunning = true;
}

//--------------------------------------------------------------
void ofApp::closeApps() {
    if (appPaths.empty()) return;
    
    ofLog() << "Closing apps...";
    for (auto& appPath : appPaths) {
        // Extract app name from path for quit command
        string appName = appPath;
        
        // Remove .app extension if present
        size_t appPos = appName.rfind(".app");
        if (appPos != string::npos) {
            appName = appName.substr(0, appPos);
        }
        
        // Get just the app name (last component of path)
        size_t lastSlash = appName.rfind('/');
        if (lastSlash != string::npos) {
            appName = appName.substr(lastSlash + 1);
        }
        
        // Use osascript to quit the app gracefully
        string command = "osascript -e 'tell application \"" + appName + "\" to quit'";
        ofLog() << "  " << command;
        system(command.c_str());
    }
    appsCurrentlyRunning = false;
}

//--------------------------------------------------------------
void ofApp::update() {
    int currentDay = getCurrentDay();
    int currentSlot = getCurrentSlot();
    
    // Only check when slot or day changes
    if (currentSlot != lastCheckedSlot || currentDay != lastCheckedDay) {
        lastCheckedSlot = currentSlot;
        lastCheckedDay = currentDay;
        
        bool shouldBeActive = schedule[currentDay][currentSlot];
        
        ofLog() << dayNames[currentDay] << " " << slotToTimeString(currentSlot) 
                << " - Slot active: " << (shouldBeActive ? "YES" : "NO")
                << " - Apps running: " << (appsCurrentlyRunning ? "YES" : "NO");
        
        if (shouldBeActive && !appsCurrentlyRunning) {
            openApps();
        } else if (!shouldBeActive && appsCurrentlyRunning) {
            closeApps();
        }
    }
}

//--------------------------------------------------------------
void ofApp::drawGrid() {
    int currentDay = getCurrentDay();
    int currentSlot = getCurrentSlot();
    
    ofSetColor(200);  // Light gray for all text
    
    // Draw title
    ofDrawBitmapString(version, 10, 20);
    ofDrawBitmapString("Click cells to toggle: [#]=RUN  [ ]=CLOSED", 10, 38);
    
    // Build header line with day names (aligned with cells)
    // Time column is "00:00 " (6 chars) + "   " (3 chars) = 9 chars before cells
    // Each cell is " [#] " = 5 chars, day names are " Mon " = 5 chars (centered)
    string header = "        ";  // 8 spaces to align with "00:00    "
    for (int d = 0; d < NUM_DAYS; d++) {
        header += " " + dayNames[d] + " ";  // 5 chars (space + Mon + space) - centered
    }
    ofDrawBitmapString(header, 5, gridStartY - 8);
    
    // Draw underline for current day
    string underline = "         ";  // 9 spaces
    for (int d = 0; d < NUM_DAYS; d++) {
        if (d == currentDay) {
            underline += " ^^^ ";  // Mark current day (5 chars)
        } else {
            underline += "     ";  // 5 spaces
        }
    }
    ofDrawBitmapString(underline, 5, gridStartY + 4);
    
    // Draw grid rows
    for (int s = 0; s < NUM_SLOTS; s++) {
        float y = gridStartY + 20 + s * cellHeight;
        
        string line = "";
        
        // Time label (show every hour)
        if (s % 2 == 0) {
            line += slotToTimeString(s);
        } else {
            line += "     ";
        }
        
        // Current time marker + extra spacing
        if (s == currentSlot) {
            line += ">  ";  // marker + 2 extra spaces
        } else {
            line += "   ";  // 3 spaces total
        }
        
        // Draw cells for each day (5 chars each: " [#] " to match " Mon ")
        for (int d = 0; d < NUM_DAYS; d++) {
            if (schedule[d][s]) {
                line += " [#] ";  // Active - apps run (5 chars, centered)
            } else {
                line += " [ ] ";  // Inactive - apps closed (5 chars, centered)
            }
        }
        
        // Highlight current row
        if (s == currentSlot) {
            line += " <-- NOW";
        }
        
        ofDrawBitmapString(line, 5, y);
    }
    
    // Draw status on multiple lines
    float statusY = gridStartY + 20 + NUM_SLOTS * cellHeight + 20;
    bool currentActive = schedule[currentDay][currentSlot];
    
    ofDrawBitmapString("Current: " + dayNames[currentDay] + " " + slotToTimeString(currentSlot), 5, statusY);
    
    string statusText;
    if (currentActive) {
        statusText = "Status:  ACTIVE (apps running)";
    } else {
        statusText = "Status:  DARKNESS (apps closed)";
    }
    ofDrawBitmapString(statusText, 5, statusY + 18);
    
    ofDrawBitmapString("Apps controlled (" + ofToString(appPaths.size()) + "):", 5, statusY + 36);
    
    // List each app
    for (int i = 0; i < appPaths.size(); i++) {
        // Extract just the app name from the path
        string appName = appPaths[i];
        size_t lastSlash = appName.rfind('/');
        if (lastSlash != string::npos) {
            appName = appName.substr(lastSlash + 1);
        }
        ofDrawBitmapString("  - " + appName, 5, statusY + 54 + i * 14);
    }
}

//--------------------------------------------------------------
// Grid is now: columns = days, rows = time slots (ASCII layout)
int ofApp::getCellDay(int x, int y) {
    // Cells start after "00:00    " which is 9 chars = 72 pixels
    float cellStartX = 5 + 9 * 8;
    if (x < cellStartX) return -1;
    int day = (x - cellStartX) / cellWidth;
    if (day < 0 || day >= NUM_DAYS) return -1;
    return day;
}

//--------------------------------------------------------------
int ofApp::getCellSlot(int x, int y) {
    // Rows start after header (gridStartY + 20)
    float rowStartY = gridStartY + 20 - cellHeight + 6;
    if (y < rowStartY) return -1;
    int slot = (y - rowStartY) / cellHeight;
    if (slot < 0 || slot >= NUM_SLOTS) return -1;
    return slot;
}

//--------------------------------------------------------------
void ofApp::draw() {
    ofBackground(0);  // Black background for ASCII art style
    drawGrid();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    if (key == 's') {
        saveSchedule();
    } else if (key == 'l') {
        loadSchedule();
    } else if (key == 'r') {
        loadAppList();
    } else if (key == 'o') {
        openApps();
    } else if (key == 'c') {
        closeApps();
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
    // Allow dragging to paint cells
    int day = getCellDay(x, y);
    int slot = getCellSlot(x, y);
    
    if (day >= 0 && slot >= 0) {
        if (day != lastDragDay || slot != lastDragSlot) {
            schedule[day][slot] = dragPaintValue;
            lastDragDay = day;
            lastDragSlot = slot;
        }
    }
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
    int day = getCellDay(x, y);
    int slot = getCellSlot(x, y);
    
    if (day >= 0 && slot >= 0) {
        // Toggle the cell
        schedule[day][slot] = !schedule[day][slot];
        
        // Store for drag painting
        dragPaintValue = schedule[day][slot];
        lastDragDay = day;
        lastDragSlot = slot;
        
        ofLog() << "Toggled " << dayNames[day] << " " << slotToTimeString(slot) 
                << " to " << (schedule[day][slot] ? "ACTIVE" : "INACTIVE");
    }
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
    // Save when done clicking/dragging
    saveSchedule();
    lastDragDay = -1;
    lastDragSlot = -1;
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
}
