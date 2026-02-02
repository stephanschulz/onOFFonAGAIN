#include "ofApp.h"

// Include auto-generated version header (created at build time)
#ifdef __has_include
#if __has_include("version.h")
#include "version.h"
#endif
#endif

#ifndef VERSION_STRING
#define VERSION_STRING "onOFFon dev-build"
#endif

//--------------------------------------------------------------
void ofApp::setup() {
    version = VERSION_STRING;
    ofLog() << "Starting " << version;
    
    // Grid layout: ASCII style - days as columns, time slots as rows
    // Using bitmap font: 8 pixels per character
    labelWidth = 6 * 8;   // "00:00 " = 6 chars
    gridStartX = 5 + labelWidth + 8;  // After time label + " "
    gridStartY = 70;      // More space for header
    cellWidth = 5 * 8;    // " [#] " = 5 chars = 40 pixels (centered with spaces)
    cellHeight = 17;      // Line height + 3 extra pixels spacing
    
    // Set window size - status panel is to the right of grid
    int gridWidth = 5 + labelWidth + 8 + (NUM_DAYS * cellWidth) + 80;  // Grid area
    int statusPanelWidth = 350;  // Space for status text on the right
    int windowWidth = gridWidth + statusPanelWidth;
    int windowHeight = gridStartY + 12 + (NUM_SLOTS * cellHeight) + 20;  // Just grid height + small margin
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
    
    // Test mode - start disabled, use real time
    testMode = false;
    testSlot = getCurrentSlot();
    testDay = getCurrentDay();
    
    // Message area
    noticeMessage = "";
    noticeStartTime = 0;
    noticeDuration = 5.0;  // Show notices for 5 seconds
    
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
    appDelays.clear();
    
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
                // Parse format: "delay, /path/to/app" or just "/path/to/app"
                int delay = 0;
                string appPath = trimmed;
                
                size_t commaPos = trimmed.find(',');
                if (commaPos != string::npos) {
                    // Has delay prefix
                    string delayStr = trimmed.substr(0, commaPos);
                    delay = ofToInt(delayStr);
                    appPath = trimmed.substr(commaPos + 1);
                    // Trim whitespace from path
                    appPath.erase(0, appPath.find_first_not_of(" \t"));
                    appPath.erase(appPath.find_last_not_of(" \t\n\r") + 1);
                }
                
                appPaths.push_back(appPath);
                appDelays.push_back(delay);
                ofLog() << "App to control: " << appPath << " (delay: " << delay << "s)";
            }
        }
        ofLog() << "Loaded " << appPaths.size() << " apps from " << path;
    } else {
        ofLogWarning() << "No appsToControl.txt found at " << path;
    }
}

//--------------------------------------------------------------
int ofApp::getCurrentDay() {
    // Return test day if in test mode
    if (testMode) {
        return testDay;
    }
    // Get current day of week (0=Mon, 6=Sun)
    time_t now = time(0);
    tm* ltm = localtime(&now);
    int dow = ltm->tm_wday;  // 0=Sunday in C
    // Convert to Mon=0, Sun=6
    return (dow == 0) ? 6 : dow - 1;
}

//--------------------------------------------------------------
int ofApp::getCurrentSlot() {
    // Return test slot if in test mode
    if (testMode) {
        return testSlot;
    }
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
bool ofApp::isAppRunning(const string& appPath) {
    // Extract app name from path
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
    
    // Use pgrep to check if app is running
    string command = "pgrep -x \"" + appName + "\" > /dev/null 2>&1";
    int result = system(command.c_str());
    return (result == 0);
}

//--------------------------------------------------------------
void ofApp::openApps() {
    if (appPaths.empty()) return;
    
    ofLog() << "Opening apps (with staggered delays)...";
    for (int i = 0; i < appPaths.size(); i++) {
        string appPath = appPaths[i];
        int delay = appDelays[i];
        
        // Check if already running
        if (isAppRunning(appPath)) {
            ofLog() << "  [SKIP] " << appPath << " (already running)";
            continue;
        }
        
        // Build command with delay - run in background with &
        string command;
        if (delay > 0) {
            // Sleep then open, all in background
            command = "(sleep " + ofToString(delay) + " && open \"" + appPath + "\") &";
        } else {
            command = "open \"" + appPath + "\"";
        }
        
        ofLog() << "  [" << delay << "s] " << appPath;
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
string ofApp::findGapsInSchedule(int day) {
    // Find gaps in the ON schedule for a given day
    // A gap is 1-2 consecutive OFF slots surrounded by ON slots
    
    vector<pair<int, int>> gaps;  // pairs of (startSlot, count)
    
    for (int s = 1; s < NUM_SLOTS - 1; s++) {
        // Check for 1 slot gap
        if (!schedule[day][s] && schedule[day][s-1] && schedule[day][s+1]) {
            gaps.push_back({s, 1});
        }
        // Check for 2 slot gap (1 hour)
        else if (s < NUM_SLOTS - 2 && 
                 !schedule[day][s] && !schedule[day][s+1] && 
                 schedule[day][s-1] && schedule[day][s+2]) {
            gaps.push_back({s, 2});
            s++;  // Skip the second gap slot
        }
    }
    
    if (gaps.empty()) {
        return "";
    }
    
    // Build message
    string msg = "NOTICE: " + dayNames[day] + " has gaps:\n";
    for (auto& gap : gaps) {
        string startTime = slotToTimeString(gap.first);
        string endTime = slotToTimeString(gap.first + gap.second);
        int minutes = gap.second * 30;
        msg += "  " + startTime + "-" + endTime + " (" + ofToString(minutes) + "min gap)\n";
    }
    msg += "Intentional?";
    
    return msg;
}

//--------------------------------------------------------------
void ofApp::checkForGaps(int day) {
    string gapMsg = findGapsInSchedule(day);
    if (!gapMsg.empty()) {
        noticeMessage = gapMsg;
        noticeStartTime = ofGetElapsedTimef();
    }
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
    
    // Clear notice after duration
    if (!noticeMessage.empty() && (ofGetElapsedTimef() - noticeStartTime) > noticeDuration) {
        noticeMessage = "";
    }
}

//--------------------------------------------------------------
void ofApp::drawGrid() {
    int currentDay = getCurrentDay();
    int currentSlot = getCurrentSlot();
    
    ofSetColor(200);  // Light gray for all text
    
    // Draw instructions
    ofDrawBitmapString("Click cells to toggle: [#]=RUN  [ ]=CLOSED", 10, 20);
    
    // Build header line with day names (aligned with cells)
    // Time column is "00:00 " (6 chars) + "   " (3 chars) = 9 chars before cells
    // Each cell is " [#] " = 5 chars, day names are " Mon " = 5 chars (centered)
    string header = "        ";  // 8 spaces to align with "00:00    "
    for (int d = 0; d < NUM_DAYS; d++) {
        header += " " + dayNames[d] + " ";  // 5 chars (space + Mon + space) - centered
    }
    ofDrawBitmapString(header, 5, gridStartY - 8);
    
    // Draw underline for current day
    string underline = "        ";  // 8 spaces
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
    
    // Draw status to the right of the grid
    float statusX = 5 + 9 * 8 + (NUM_DAYS * cellWidth) + 30;
    float statusY = gridStartY + 20;
    bool currentActive = schedule[currentDay][currentSlot];
    
    string timeLabel = "Current: " + dayNames[currentDay] + " " + slotToTimeString(currentSlot);
    ofDrawBitmapString(timeLabel, statusX, statusY);
    
    if (testMode) {
        ofDrawBitmapString("[TEST MODE - arrows to change, 't' to exit]", statusX, statusY + 18);
    }
    
    string statusText;
    if (currentActive) {
        statusText = "Status: ACTIVE (apps running)";
    } else {
        statusText = "Status: DARKNESS (apps closed)";
    }
    ofDrawBitmapString(statusText, statusX, statusY + 36);
    
    ofDrawBitmapString("Apps controlled (" + ofToString(appPaths.size()) + "):", statusX, statusY + 60);
    
    // List each app with delay
    for (int i = 0; i < appPaths.size(); i++) {
        // Extract just the app name from the path
        string appName = appPaths[i];
        size_t lastSlash = appName.rfind('/');
        if (lastSlash != string::npos) {
            appName = appName.substr(lastSlash + 1);
        }
        string delayStr = "[" + ofToString(appDelays[i]) + "s] ";
        ofDrawBitmapString("  " + delayStr + appName, statusX, statusY + 78 + i * 14);
    }
    
    // Draw notice message below the status
    if (!noticeMessage.empty()) {
        float msgX = statusX;
        float msgY = statusY + 78 + appPaths.size() * 14 + 30;
        
        // Fade out effect
        float elapsed = ofGetElapsedTimef() - noticeStartTime;
        float alpha = 255;
        if (elapsed > noticeDuration - 1.0) {
            alpha = 255 * (noticeDuration - elapsed);
        }
        
        // Yellow/orange warning color
        ofSetColor(255, 200, 50, (int)alpha);
        
        // Draw multiline message
        vector<string> lines = ofSplitString(noticeMessage, "\n");
        for (int i = 0; i < lines.size(); i++) {
            ofDrawBitmapString(lines[i], msgX, msgY + i * 16);
        }
        
        ofSetColor(200);  // Reset color
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
    
    // Version number bottom right (grey on black)
    ofSetColor(100);  // Dark grey
    float versionX = ofGetWidth() - version.length() * 8 - 10;
    float versionY = ofGetHeight() - 10;
    ofDrawBitmapString(version, versionX, versionY);
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
    } else if (key == 't') {
        // Toggle test mode
        testMode = !testMode;
        if (testMode) {
            // Initialize test time to current real time
            time_t now = time(0);
            tm* ltm = localtime(&now);
            int dow = ltm->tm_wday;
            testDay = (dow == 0) ? 6 : dow - 1;
            testSlot = ltm->tm_hour * 2 + (ltm->tm_min >= 30 ? 1 : 0);
            ofLog() << "TEST MODE ON - Use arrow keys to change time";
        } else {
            ofLog() << "TEST MODE OFF - Using real time";
            // Reset state so it re-evaluates with real time
            lastCheckedSlot = -1;
            lastCheckedDay = -1;
        }
    } else if (key == OF_KEY_UP) {
        // Move time slot earlier
        if (testMode) {
            testSlot--;
            if (testSlot < 0) testSlot = NUM_SLOTS - 1;
            lastCheckedSlot = -1;  // Force re-check
            ofLog() << "Test time: " << dayNames[testDay] << " " << slotToTimeString(testSlot);
        }
    } else if (key == OF_KEY_DOWN) {
        // Move time slot later
        if (testMode) {
            testSlot++;
            if (testSlot >= NUM_SLOTS) testSlot = 0;
            lastCheckedSlot = -1;  // Force re-check
            ofLog() << "Test time: " << dayNames[testDay] << " " << slotToTimeString(testSlot);
        }
    } else if (key == OF_KEY_LEFT) {
        // Move to previous day
        if (testMode) {
            testDay--;
            if (testDay < 0) testDay = NUM_DAYS - 1;
            lastCheckedDay = -1;  // Force re-check
            ofLog() << "Test time: " << dayNames[testDay] << " " << slotToTimeString(testSlot);
        }
    } else if (key == OF_KEY_RIGHT) {
        // Move to next day
        if (testMode) {
            testDay++;
            if (testDay >= NUM_DAYS) testDay = 0;
            lastCheckedDay = -1;  // Force re-check
            ofLog() << "Test time: " << dayNames[testDay] << " " << slotToTimeString(testSlot);
        }
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
    
    // Check for gaps if we were painting ON values
    if (lastDragDay >= 0 && dragPaintValue) {
        checkForGaps(lastDragDay);
    }
    
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
