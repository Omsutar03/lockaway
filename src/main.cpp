#include <iostream>
#include <string>
#include <fstream>
#include <utility>
#include <chrono>
#include <thread>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

// Include Windows API headers
#include <windows.h>
#include <psapi.h>

std::pair<std::string, std::string> lastActiveWindowInfo; // Stores info of the last active window
const std::string configFileName = "Locked";
bool showAddedMessage = false;
bool showRemovedMessage = false;
bool showEmptyPasswordMessage = false;
bool showAlreadyAddedMessage = false; // Flag for "Already Added" message
bool showNotFoundInListMessage = false;
bool showSaveMessage = false; // Flag for "Save" message
int inputSeconds = 0; // Variable to store the input number of seconds
std::chrono::time_point<std::chrono::steady_clock> messageStartTime;

bool showConfigWindow = true;
bool showPasswordWindow = false; // Controls visibility of the password window
char passwordInput[256] = "";    // Stores user input for the password
char setPassword[256] = "";
std::string passwordToCheck;
bool incorrectPassword = false;  // Flag to show incorrect password message
bool showGUI = false;

// Global variable to store the hook handle
HHOOK hKeyboardHook = nullptr;

// Low-level keyboard hook procedure
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;

        // Block the Win key (left and right)
        if (pKeyInfo->vkCode == VK_LWIN || pKeyInfo->vkCode == VK_RWIN) {
            return 1; // Block the key
        }

        // Block the Alt key (left and right)
        if (pKeyInfo->vkCode == VK_LMENU || pKeyInfo->vkCode == VK_RMENU) {
            return 1; // Block the key
        }
    }

    // Pass the event to the next hook
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// Function to install the keyboard hook
void InstallKeyboardHook() {
    if (hKeyboardHook) {
        std::cerr << "Keyboard hook is already installed! Handle: " << hKeyboardHook << std::endl;
        return;
    }
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!hKeyboardHook) {
        std::cerr << "Failed to install keyboard hook! Error: " << std::endl;
    }
}

// Function to uninstall the keyboard hook
void UninstallKeyboardHook() {
    if (hKeyboardHook) {
        BOOL result = UnhookWindowsHookEx(hKeyboardHook);
        if (!result) {
            std::cerr << "Failed to uninstall keyboard hook! Error: " << std::endl;
        }
        hKeyboardHook = nullptr;
    }
}

// Function to get the idle time in seconds
DWORD getIdleTimeInSeconds() {
    LASTINPUTINFO lastInputInfo = { sizeof(LASTINPUTINFO) };
    if (GetLastInputInfo(&lastInputInfo)) {
        DWORD currentTime = GetTickCount();
        return (currentTime - lastInputInfo.dwTime) / 1000; // Convert to seconds
    }
    return 0;
}

// Function to read the Locked file and return its content
std::string readLockedFile() {
    std::ifstream file(configFileName);
    std::string content, line;

    if (file.is_open()) {
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        file.close();
    }
    return content.empty() ? "No content in Locked file." : content;
}

// Function to get the current active application's name and window title
std::pair<std::string, std::string> getCurrentAppAndWindow() {
    char windowTitle[256] = {0};
    char exePath[MAX_PATH] = {0};

    HWND hwnd = GetForegroundWindow(); // Get the active window handle
    if (!hwnd) return {"", ""};

    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle)); // Get the window title

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId); // Get the process ID of the active window

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (processHandle) {
        GetModuleFileNameExA(processHandle, nullptr, exePath, sizeof(exePath)); // Get the full executable path
        CloseHandle(processHandle);
    }

    // Extract the executable name from the path
    std::string exeName = exePath;
    size_t pos = exeName.find_last_of("\\/");
    if (pos != std::string::npos) {
        exeName = exeName.substr(pos + 1);
    }

    // Convert the executable name to lowercase
    std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::tolower);

    return {exeName, windowTitle}; // Return the executable name and window title
}

void monitorInactivity() {
    while (true) {
        DWORD idleTime = getIdleTimeInSeconds();

        // Read the idle timeout from the Locked file
        std::ifstream file(configFileName);
        int timeoutSeconds = 0;
        std::vector<std::tuple<std::string, std::string, std::string>> lockedApps;

        if (file.is_open()) {
            std::string line;
            std::getline(file, line); // Skip first line (# Locked Applications)
            if (std::getline(file, line)) { // Read the second line (timeout seconds)
                timeoutSeconds = std::stoi(line);
            }
            while (std::getline(file, line)) { // Read locked applications and window titles
                size_t firstSeparatorPos = line.find(" | ");
                if (firstSeparatorPos != std::string::npos) {
                    size_t secondSeparatorPos = line.find(" | ", firstSeparatorPos + 3);
                    std::string appName = line.substr(0, firstSeparatorPos);
                    std::string windowTitle = line.substr(firstSeparatorPos + 3, secondSeparatorPos - (firstSeparatorPos + 3));
                    std::string windowPassword = line.substr(secondSeparatorPos + 3);
                    lockedApps.emplace_back(appName, windowTitle, windowPassword);
                }
            }
            file.close();
        }

        // If the user is idle for the specified time
        if (timeoutSeconds > 0 && idleTime >= timeoutSeconds) {
            auto [currentApp, currentWindow] = getCurrentAppAndWindow();

            // Check if the current application is in the Locked list
            for (const auto& [lockedApp, lockedWindow, lockedWindowPassword] : lockedApps) {
                if (lockedApp == currentApp && lockedWindow == currentWindow) {
                    // Show the password window
                    passwordToCheck = lockedWindowPassword;
                    showGUI = !showGUI;
                    showConfigWindow = false;
                    showPasswordWindow = true;
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Check every second
    }
}

// Function to ensure the Locked file exists
void ensureConfigFile() {
    std::ifstream file(configFileName);
    if (!file.good()) {
        std::ofstream createFile(configFileName);
        if (createFile.is_open()) {
            createFile << "# Locked Applications\n";
            createFile << "0\n"; // Default seconds
            createFile.close();
        }
    }
}

// Function to check if the entry already exists in the Locked file
bool isAlreadyAdded(const std::string& appName, const std::string& windowTitle) {
    std::ifstream file(configFileName);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t firstSeparatorPos = line.find(" | ");
            if (firstSeparatorPos != std::string::npos) {
                size_t secondSeparatorPos = line.find(" | ", firstSeparatorPos + 3);
                std::string appAndWindow = line.substr(0, secondSeparatorPos);
                if (appAndWindow == appName + " | " + windowTitle) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Function to add a new entry to the Locked file
void addToConfigFile(const std::string& appName, const std::string& windowTitle, const char windowPassword[256]) {
    // Ensure the file has the correct format before appending
    ensureConfigFile();

    if (isAlreadyAdded(appName, windowTitle)) {
        // If already added, show "Already Added" message
        showAlreadyAddedMessage = true;
        messageStartTime = std::chrono::steady_clock::now();
    } else {
        // Read the file to ensure lines 1 and 2 are correct
        std::ifstream file(configFileName);
        std::vector<std::string> lines;

        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(line);
            }
            file.close();
        }

        // Ensure the first line is "# Locked Applications"
        if (lines.empty() || lines[0] != "# Locked Applications") {
            if (lines.empty()) {
                lines.push_back("# Locked Applications");
            } else {
                lines[0] = "# Locked Applications";
            }
        }

        // Ensure the second line is present (if not, set it to a default value)
        if (lines.size() < 2) {
            lines.push_back("0");
        }

        // Append the new application info starting from the third line
        lines.push_back(appName + " | " + windowTitle + " | " + windowPassword);

        // Write the updated lines back to the file
        std::ofstream outFile(configFileName, std::ios::trunc);
        if (outFile.is_open()) {
            for (const auto& line : lines) {
                outFile << line << "\n";
            }
            outFile.close();

            // Show "Added!" message
            showAddedMessage = true;
            messageStartTime = std::chrono::steady_clock::now();
        }
    }
}

// Function to remove entry from the Locked file
void removeFromConfigFile(const std::string& appName, const std::string& windowTitle, const char windowPassword[256]) {
    bool fileFound = false;
    // Ensure the file exists
    ensureConfigFile();

    // Read the file and store its lines in a vector
    std::ifstream file(configFileName);
    std::vector<std::string> lines;

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
    }

    // Create the entry string to search for
    std::string entryToRemove = appName + " | " + windowTitle + " | " + windowPassword;

    // Reconstruct the file by excluding the entry
    std::ofstream outFile(configFileName, std::ios::trunc);

    if (outFile.is_open()) {
        for (const auto& line : lines) {
            if (line != entryToRemove) {
                outFile << line << "\n"; // Write the line back to the file if it's not the entry to remove
            } else {
                fileFound = true;
            }
        }
        outFile.close();

        if (fileFound) {
            showRemovedMessage = true;
            messageStartTime = std::chrono::steady_clock::now();
        } else {
            // Entry not found
            showNotFoundInListMessage = true;
            messageStartTime = std::chrono::steady_clock::now();

        }
    }

}

// Function to get the active window's executable name and window title
std::pair<std::string, std::string> getActiveWindowInfo() {
    char windowTitle[256] = {0};
    char exePath[MAX_PATH] = {0};

    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return {"Unknown", "Unknown"};

    GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (processHandle) {
        GetModuleFileNameExA(processHandle, nullptr, exePath, sizeof(exePath));
        CloseHandle(processHandle);
    }

    std::string exeName = exePath;
    size_t pos = exeName.find_last_of("\\/");
    if (pos != std::string::npos) {
        exeName = exeName.substr(pos + 1);
    }

    // Convert the executable name to lowercase
    std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::tolower);

    return {exeName, windowTitle};
}

// Function to handle hotkey and capture the active window info
void handleHotkey() {
    lastActiveWindowInfo = getActiveWindowInfo(); // Store the info of the currently active window
}

// Function to save the input seconds to a file or use in any other logic
void saveSeconds() {
    std::ifstream file(configFileName);
    std::vector<std::string> lines;

    // Read the file content
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
    }

    // Ensure the first line is "# Locked Applications"
    if (lines.empty() || lines[0] != "# Locked Applications") {
        if (lines.empty()) {
            lines.push_back("# Locked Applications");
        } else {
            lines[0] = "# Locked Applications";
        }
    }

    // Ensure the second line is the seconds input
    if (lines.size() < 2) {
        lines.push_back(std::to_string(inputSeconds));
    } else {
        lines[1] = std::to_string(inputSeconds);
    }

    // Write the updated lines back to the file
    std::ofstream outFile(configFileName, std::ios::trunc);
    if (outFile.is_open()) {
        for (const auto& line : lines) {
            outFile << line << "\n";
        }
        outFile.close();

        // Show a "Saved!" message
        showSaveMessage = true;
        messageStartTime = std::chrono::steady_clock::now();
    }
}

void renderConfigWindow() {
    if (!showConfigWindow) return;
    
    ImGui::Begin("Configuration Window", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::BeginChild("Active Window Information", ImVec2(0, 50), true);
    ImGui::Text("Application: %s", lastActiveWindowInfo.first.c_str());
    ImGui::Text("Window Title: %s", lastActiveWindowInfo.second.c_str());
    ImGui::EndChild();

    // Add a separator between frames
    ImGui::Separator();

    ImGui::BeginChild("Add or Remove from Locked list", ImVec2(0, 120), true);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Enter Password: ");

    ImGui::InputText("Password", setPassword, IM_ARRAYSIZE(setPassword), ImGuiInputTextFlags_Password);

    if (ImGui::Button("+")) {
        if (strlen(setPassword) > 0) {
            // Add current displayed info to the config file
            addToConfigFile(lastActiveWindowInfo.first, lastActiveWindowInfo.second, setPassword);

            // Clear the password input field
            memset(setPassword, 0, sizeof(setPassword)); // Reset the buffer to an empty string
        } else if (strlen(setPassword) == 0) {
            showEmptyPasswordMessage = true;
            messageStartTime = std::chrono::steady_clock::now();
        }
        
    }

    if (ImGui::Button("-")) {
        if (strlen(setPassword) > 0) {
            // Add current displayed info to the config file
            removeFromConfigFile(lastActiveWindowInfo.first, lastActiveWindowInfo.second, setPassword);

            // Clear the password input field
            memset(setPassword, 0, sizeof(setPassword)); // Reset the buffer to an empty string
        } else if (strlen(setPassword) == 0) {
            showEmptyPasswordMessage = true;
            messageStartTime = std::chrono::steady_clock::now();
        }
        
    }

    // Show "Empty Password" message if the flag is set
    if (showEmptyPasswordMessage) {
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - messageStartTime).count();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Password cannot be empty!");
        // Hide the message after 2 seconds
        if (elapsedTime > 1.0f) {
            showEmptyPasswordMessage = false;
        }
    }

    // Show "Added" message if the flag is set
    if (showAddedMessage) {
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - messageStartTime).count();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Added to Locked List!");

        // Hide the message after 2 seconds
        if (elapsedTime > 1.0f) {
            showAddedMessage = false;
        }
    }

    // Show "Removed" message if the flag is set
    if (showRemovedMessage) {
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - messageStartTime).count();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Removed from Locked List!");

        // Hide the message after 2 seconds
        if (elapsedTime > 1.0f) {
            showRemovedMessage = false;
        }
    }

    // Show "Already Added" message if the flag is set
    if (showAlreadyAddedMessage) {
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - messageStartTime).count();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Already Added to Locked List!");

        // Hide the message after 2 seconds
        if (elapsedTime > 1.0f) {
            showAlreadyAddedMessage = false;
        }
    }

    // Show "Not found in List" message if the flag is set
    if (showNotFoundInListMessage) {
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - messageStartTime).count();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not found in Locked List (or password might be incorrect)!");

        // Hide the message after 2 seconds
        if (elapsedTime > 1.0f) {
            showNotFoundInListMessage = false;
        }
    }


    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginChild("Set Inactivity Time Limit", ImVec2(0, 100), true);
    
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Set Inactivity Time Limit: ");

    // Input for number of seconds
    ImGui::InputInt("Seconds", &inputSeconds);

    // Save button
    if (ImGui::Button("Save")) {
        saveSeconds(); // Save the number of seconds
    }

    // Show "Saved" message if the flag is set
    if (showSaveMessage) {
        auto now = std::chrono::steady_clock::now();
        float elapsedTime = std::chrono::duration<float>(now - messageStartTime).count();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Saved!");

        // Hide the message after 2 seconds
        if (elapsedTime > 1.0f) {
            showSaveMessage = false;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void renderPasswordWindow() {
    if (!showPasswordWindow) return;

    // Set the window to always be on top
    ImGui::SetNextWindowFocus();

    // Begin the password window with no close button
    ImGui::Begin("Unlock Application", nullptr, ImGuiWindowFlags_NoCollapse);

    // Display instructions
    ImGui::Text("You have been inactive for too long.");
    ImGui::Text("Please enter the password to continue:");

    // Password input field
    ImGui::InputText("Password", passwordInput, IM_ARRAYSIZE(passwordInput), ImGuiInputTextFlags_Password);

    // Incorrect password message
    if (incorrectPassword) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Incorrect password! Please try again.");
    }

    // Submit button
    if (ImGui::Button("Submit")) {
        if (std::string(passwordInput) == passwordToCheck) {
            // Correct password: close the window
            showGUI = !showGUI;
            showConfigWindow = true;
            showPasswordWindow = false;
            incorrectPassword = false;
            memset(passwordInput, 0, sizeof(passwordInput)); // Clear the input field

            // Uninstall the keyboard hook when the password window is closed
            UninstallKeyboardHook();
        } else {
            // Incorrect password: show error message
            incorrectPassword = true;
        }
    }

    ImGui::End();
}

int main() {
    // Start the inactivity monitoring thread
    std::thread inactivityThread(monitorInactivity);
    inactivityThread.detach();

    // Ensure the config file exists
    ensureConfigFile();

    // Initialize GLFW
    if (!glfwInit()) return -1;

    // Create a windowed mode window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lockaway", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    RegisterHotKey(NULL, 1, MOD_WIN | MOD_SHIFT, 0x4C); // WIN + SHIFT + L

    // Store the original windowed mode dimensions and position
    int windowedWidth = 800;
    int windowedHeight = 600;
    int windowedPosX, windowedPosY;
    glfwGetWindowPos(window, &windowedPosX, &windowedPosY);

    // Get the primary monitor and its video mode
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    while (!glfwWindowShouldClose(window)) {
        MSG msg = {0};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY && msg.wParam == 1) {
                // Ignore the hotkey if the password window is visible
                if (!showPasswordWindow) {
                    handleHotkey(); // Capture the active window info
                    showGUI = !showGUI; // Toggle GUI visibility
                }
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        glfwPollEvents();

        if (!showGUI) {
            glfwHideWindow(window);
            continue;
        } else {
            glfwShowWindow(window);
        }

        // Switch between windowed and borderless full-screen modes based on the flags
        if (showConfigWindow) {
            // Switch to windowed mode
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE); // Restore window decorations
            glfwSetWindowMonitor(window, NULL, windowedPosX, windowedPosY, windowedWidth, windowedHeight, mode->refreshRate);

            // Uninstall the keyboard hook when the password window is not active
            UninstallKeyboardHook();
        } else if (showPasswordWindow) {
            // Switch to borderless full-screen mode
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE); // Remove window decorations
            glfwSetWindowPos(window, 0, 0); // Position the window at the top-left corner
            glfwSetWindowSize(window, mode->width, mode->height); // Set the window size to match the monitor resolution

            // Install the keyboard hook when the password window is active
            InstallKeyboardHook();
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render the Config window (main)
        renderConfigWindow();
        // Render the password window
        renderPasswordWindow();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    UninstallKeyboardHook(); // Uninstall the keyboard hook
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}