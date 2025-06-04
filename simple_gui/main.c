#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include <assert.h>
#include <ctype.h>
#include <strings.h>

// Assertion macro
#define c_assert(e) ((e) ? (true) : (printf("%s,%d: assertion '%s' failed\n", __FILE__, __LINE__, #e), false))

// Constants
#define MAX_FILES 16
#define FILENAME_LENGTH 256

// Global variables (try to minimize these)
char filenames[MAX_FILES][FILENAME_LENGTH];
int fileCount = 0;
int selectedFileIndex = -1; // Initially no file is selected
Texture2D textures[MAX_FILES]; // Array to store loaded textures
float filenameWidths[MAX_FILES]; // Store filename widths

// Scrollbar structure
typedef struct {
    Rectangle bounds;
    Rectangle knob;
    float value;
    float horizontalValue; // Horizontal scroll value
    Rectangle horizontalBounds; // Horizontal scrollbar bounds
    Rectangle horizontalKnob; // Horizontal scrollbar knob
    float verticalKnobHeight;
    float horizontalKnobWidth;
} ScrollBar;

// Function prototypes
void initScrollBar(ScrollBar* scrollBar, Rectangle dropArea);
void updateScrollBar(ScrollBar* scrollBar, Rectangle dropArea, float totalFilenamesHeight, float maxFilenameWidth);
void drawScrollBar(ScrollBar scrollBar);
void unloadAllTextures();

// Helper function to check if a file has an image extension
bool isImageFile(const char* filename) {
    const char* extensions[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        const char* ext = extensions[i];
        size_t filenameLength = strlen(filename);
        size_t extLength = strlen(ext);
        if (filenameLength >= extLength && strcasecmp(filename + filenameLength - extLength, ext) == 0) {
            return true;
        }
    }
    TraceLog(LOG_INFO, "File %s is NOT an image", filename);
    return false;
}

// Function to check if a filename already exists in the array
bool filenameExists(const char* filename, char filenames[][FILENAME_LENGTH], int fileCount) {
    c_assert(filename != NULL);
    c_assert(fileCount >= 0);

    for (int i = 0; i < fileCount; i++) {
        if (strcmp(filenames[i], filename) == 0) {
            return true; // Filename already exists
        }
    }
    return false; // Filename does not exist
}

// Function to handle file dropping
void handleFileDrop(void) {
    FilePathList droppedFiles = LoadDroppedFiles();
    if (droppedFiles.count > 0) {
        for (int i = 0; i < droppedFiles.count && fileCount < MAX_FILES; i++) {
            if (!filenameExists(droppedFiles.paths[i], filenames, fileCount)) {
                strncpy(filenames[fileCount], droppedFiles.paths[i], FILENAME_LENGTH - 1);
                filenames[fileCount][FILENAME_LENGTH - 1] = '\0'; // Ensure null termination

                // Load texture if the file is an image
                if (isImageFile(filenames[fileCount])) {
                    textures[fileCount] = LoadTexture(filenames[fileCount]);
                    TraceLog(LOG_INFO, "Texture loaded for %s with ID %d", filenames[fileCount], textures[fileCount].id);
                    if (textures[fileCount].id == 0) {
                        TraceLog(LOG_ERROR, "Failed to load texture: %s", filenames[fileCount]);
                    }
                } else {
                    textures[fileCount].id = 0; // Mark as not an image
                }

                // Measure and store filename width
                const int filenameTextHeight = 10;
                filenameWidths[fileCount] = MeasureText(filenames[fileCount], filenameTextHeight);

                fileCount++;
            }
        }
    }
    UnloadDroppedFiles(droppedFiles); // Unload filepaths from memory
}

// Function to initialize the scrollbar
void initScrollBar(ScrollBar* scrollBar, Rectangle dropArea) {
    scrollBar->bounds.x = dropArea.x;
    scrollBar->bounds.y = dropArea.y + dropArea.height - 20;
    scrollBar->bounds.width = dropArea.width;
    scrollBar->bounds.height = 20;
    scrollBar->value = 0.0f;

    // Initialize horizontal scrollbar
    scrollBar->horizontalBounds.x = dropArea.x;
    scrollBar->horizontalBounds.y = dropArea.y + dropArea.height - 40;
    scrollBar->horizontalBounds.width = dropArea.width - 20;
    scrollBar->horizontalBounds.height = 20;
    scrollBar->horizontalValue = 0.0f;
    scrollBar->verticalKnobHeight = 0.0f;
    scrollBar->horizontalKnobWidth = 0.0f;
}

// Function to update the scrollbar
void updateScrollBar(ScrollBar* scrollBar, Rectangle dropArea, float totalFilenamesHeight, float maxFilenameWidth) {
    Vector2 mousePosition = GetMousePosition();

    // Vertical Scrollbar
    // Calculate the maximum scrollable height
    float maxScrollableHeight = totalFilenamesHeight - dropArea.height + 20; // +20 for scrollbar height
    if (maxScrollableHeight < 0) maxScrollableHeight = 0;

    // Calculate the proportional height of the scrollbar knob
    float knobHeight = dropArea.height / (float)totalFilenamesHeight * (dropArea.height - 20);
    if (knobHeight > dropArea.height - 20) knobHeight = dropArea.height - 20;
    if (knobHeight < 0) knobHeight = 0;

    // Update scrollbar value based on mouse interaction
    if (CheckCollisionPointRec(mousePosition, scrollBar->bounds) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        scrollBar->value = (mousePosition.y - scrollBar->bounds.y) / (scrollBar->bounds.height - knobHeight);
        if (scrollBar->value < 0.0f) scrollBar->value = 0.0f;
        if (scrollBar->value > 1.0f) scrollBar->value = 1.0f;
    }

    // Update scrollbar value based on mouse wheel
    float wheelMove = GetMouseWheelMove();
    if (CheckCollisionPointRec(mousePosition, dropArea) && wheelMove != 0) {
        scrollBar->value -= wheelMove * 0.1f;
        if (scrollBar->value < 0.0f) scrollBar->value = 0.0f;
        if (scrollBar->value > 1.0f) scrollBar->value = 1.0f;
    }

    // Update scrollbar knob position
    scrollBar->knob.x = scrollBar->bounds.x;
    scrollBar->knob.y = scrollBar->bounds.y + scrollBar->value * (scrollBar->bounds.height - knobHeight);
    scrollBar->knob.width = scrollBar->bounds.width;
    scrollBar->knob.height = knobHeight;
    scrollBar->verticalKnobHeight = knobHeight;

    // Horizontal Scrollbar
    // Calculate the maximum scrollable width
    float maxScrollableWidth = maxFilenameWidth - dropArea.width;
    if (maxScrollableWidth < 0) maxScrollableWidth = 0;

    // Calculate the proportional width of the horizontal scrollbar knob
    float knobWidth = dropArea.width / (float)maxFilenameWidth * (dropArea.width - 20);
    if (knobWidth > dropArea.width - 20) knobWidth = dropArea.width - 20;
    if (knobWidth < 0) knobWidth = 0;

    // Update horizontal scrollbar value based on mouse interaction
    if (CheckCollisionPointRec(mousePosition, scrollBar->horizontalBounds) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        scrollBar->horizontalValue = (mousePosition.x - scrollBar->horizontalBounds.x) / (scrollBar->horizontalBounds.width - knobWidth);
        if (scrollBar->horizontalValue < 0.0f) scrollBar->horizontalValue = 0.0f;
        if (scrollBar->horizontalValue > 1.0f) scrollBar->horizontalValue = 1.0f;
    }

    // Update horizontal scrollbar knob position
    scrollBar->horizontalKnob.x = scrollBar->horizontalBounds.x + scrollBar->horizontalValue * (scrollBar->horizontalBounds.width - knobWidth);
    scrollBar->horizontalKnob.y = scrollBar->horizontalBounds.y;
    scrollBar->horizontalKnob.width = knobWidth;
    scrollBar->horizontalKnob.height = scrollBar->horizontalBounds.height;
    scrollBar->horizontalKnobWidth = knobWidth;
}

// Function to draw the scrollbar
void drawScrollBar(ScrollBar scrollBar) {
    DrawRectangleRec(scrollBar.bounds, DARKGRAY);
    DrawRectangleRec(scrollBar.knob, LIGHTGRAY);

    // Draw horizontal scrollbar
    DrawRectangleRec(scrollBar.horizontalBounds, DARKGRAY);
    DrawRectangleRec(scrollBar.horizontalKnob, LIGHTGRAY);
}

// Function to update the game state
void update(Rectangle* dropArea, ScrollBar* scrollBar, float* totalFilenamesHeight, float* maxFilenameWidth, bool* showScrollBar, bool* showHorizontalScrollBar) {
    c_assert(dropArea != NULL);
    c_assert(scrollBar != NULL);

    int currentScreenWidth = GetScreenWidth();
    int currentScreenHeight = GetScreenHeight();
    Vector2 mousePosition = GetMousePosition();

    // Update drop area size in case of window resize
    dropArea->width = currentScreenWidth / 4;
    dropArea->height = currentScreenHeight;

    // Update scrollbar position
    scrollBar->bounds.x = dropArea->x;
    scrollBar->bounds.y = dropArea->y + dropArea->height - 20;
    scrollBar->bounds.width = dropArea->width;

    // Update horizontal scrollbar position
    scrollBar->horizontalBounds.x = dropArea->x;
    scrollBar->horizontalBounds.y = dropArea->y + dropArea->height - 40;
    scrollBar->horizontalBounds.width = dropArea->width - 20;

    // Check if a file has been dropped
    if (IsFileDropped()) {
        handleFileDrop();
    }

    // Calculate total height of filenames
    const int filenameTextHeight = 10; // Approximate text height
    *totalFilenamesHeight = 0;
    *maxFilenameWidth = 0;
    for (int i = 0; i < fileCount; i++) {
        *totalFilenamesHeight += filenameTextHeight + 10; // Add some padding
        if (filenameWidths[i] > *maxFilenameWidth) {
            *maxFilenameWidth = filenameWidths[i];
        }
    }

    // Update scrollbar
    updateScrollBar(scrollBar, *dropArea, *totalFilenamesHeight, *maxFilenameWidth);

    // Check for clicks on filenames
    if (CheckCollisionPointRec(mousePosition, *dropArea) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Calculate scroll offset
        float maxScroll = *totalFilenamesHeight - dropArea->height + 20;
        if (maxScroll < 0) maxScroll = 0;
        float scrollOffset = -scrollBar->value * maxScroll;

        // Calculate the index of the clicked filename
        float currentY = dropArea->y + 30 + scrollOffset;
        for (int i = 0; i < fileCount; i++) {
            // Check collision with the entire filename height
            if (mousePosition.y > currentY && mousePosition.y < currentY + filenameTextHeight + 10) {
                // Unload previously selected texture, if any
                if (selectedFileIndex != -1 && textures[selectedFileIndex].id > 0) {
                    UnloadTexture(textures[selectedFileIndex]);
                    TraceLog(LOG_INFO, "Unloaded texture for %s with ID %d", filenames[selectedFileIndex], textures[selectedFileIndex].id);
                    textures[selectedFileIndex].id = 0; // Reset texture ID
                }
                selectedFileIndex = i;

                // Load texture if it was unloaded
                if (isImageFile(filenames[selectedFileIndex]) && textures[selectedFileIndex].id == 0) {
                    textures[selectedFileIndex] = LoadTexture(filenames[selectedFileIndex]);
                    TraceLog(LOG_INFO, "Texture loaded for %s with ID %d", filenames[selectedFileIndex], textures[selectedFileIndex].id);
                    if (textures[selectedFileIndex].id == 0) {
                        TraceLog(LOG_ERROR, "Failed to load texture: %s", filenames[selectedFileIndex]);
                    }
                }
                break;
            }
            currentY += filenameTextHeight + 10;
        }
    }

    // Determine scrollbar visibility
    float maxScroll = *totalFilenamesHeight - dropArea->height + 20;
    *showScrollBar = maxScroll > 0;

    // Determine horizontal scrollbar visibility
    *showHorizontalScrollBar = *maxFilenameWidth > dropArea->width;
}

// Function to draw the game
void draw(Rectangle dropArea, ScrollBar scrollBar, float totalFilenamesHeight, float maxFilenameWidth, bool showScrollBar, bool showHorizontalScrollBar) {
    int currentScreenWidth = GetScreenWidth();
    int currentScreenHeight = GetScreenHeight();
    const int fontSize = 20;
    const int filenameTextHeight = 10;

    // Calculate scroll offset
    float maxScroll = totalFilenamesHeight - dropArea.height + 20;
    if (maxScroll < 0) maxScroll = 0;
    float scrollOffset = -scrollBar.value * maxScroll;

    // Calculate horizontal scroll offset
    float maxHorizontalScroll = maxFilenameWidth - dropArea.width;
    if (maxHorizontalScroll < 0) maxHorizontalScroll = 0;
    float horizontalScrollOffset = -scrollBar.horizontalValue * maxHorizontalScroll;

    // Draw
    BeginDrawing();

    ClearBackground(RAYWHITE);

    // Draw file drop area
    DrawRectangleRec(dropArea, LIGHTGRAY);
    DrawText("Drop files here", dropArea.x + 10, dropArea.y + 10, 20, GRAY);

    // Draw dropped filenames with scroll offset
    BeginScissorMode(dropArea.x, dropArea.y, dropArea.width, dropArea.height - 20); // Apply scissor test
    float currentY = dropArea.y + 30 + scrollOffset;
    for (int i = 0; i < fileCount; i++) {
        // Highlight selected filename
        if (i == selectedFileIndex) {
            DrawRectangle(dropArea.x + 10 + horizontalScrollOffset, currentY, filenameWidths[i], filenameTextHeight, ORANGE);
        }

        DrawText(filenames[i], dropArea.x + 10 + horizontalScrollOffset, currentY, filenameTextHeight, DARKGRAY);
        currentY += filenameTextHeight + 10;
    }
    EndScissorMode();

    // Draw scrollbar only if content exceeds drop area height
    if (showScrollBar) {
        drawScrollBar(scrollBar);
    }

    // Draw horizontal scrollbar only if content exceeds drop area width
    if (showHorizontalScrollBar) {
        drawScrollBar(scrollBar);
    }

    // Display selected filename or image, centered in the right segment
    if (selectedFileIndex != -1) {
        const char* selectedFilename = filenames[selectedFileIndex];

        // Calculate the available space for the image
        int availableWidth = currentScreenWidth - dropArea.width;
        int availableHeight = currentScreenHeight;
        Rectangle imageArea = { dropArea.width, 0, availableWidth, availableHeight };

        // Check if the selected file is an image
        if (isImageFile(selectedFilename)) {
            // Check if the texture is loaded
            if (textures[selectedFileIndex].id > 0) {
                // Calculate the position to center the image
                //int imageX = dropArea.width + (availableWidth - textures[selectedFileIndex].width) / 2;
                //int imageY = (availableHeight - textures[selectedFileIndex].height) / 2;

                // Scale the image down if it's too large
                float scale = 1.0f;
                if (textures[selectedFileIndex].width > availableWidth) {
                    scale = (float)availableWidth / textures[selectedFileIndex].width;
                }
                if (textures[selectedFileIndex].height > availableHeight) { // Corrected this line
                    scale = (float)availableHeight / textures[selectedFileIndex].height;
                }

                // Draw the image, scaled if necessary
                Rectangle sourceRec = { 0.0f, 0.0f, (float)textures[selectedFileIndex].width, (float)textures[selectedFileIndex].height };
                
                // Calculate the final destination rectangle
                Rectangle destRec = {
                    (float)(dropArea.width + availableWidth / 2 - (textures[selectedFileIndex].width * scale) / 2),
                    (float)(availableHeight / 2 - (textures[selectedFileIndex].height * scale) / 2),
                    (float)textures[selectedFileIndex].width * scale,
                    (float)textures[selectedFileIndex].height * scale
                };
                
                Vector2 origin = { 0.0f, 0.0f };

                // Use scissor mode to clip the image within the right segment
                BeginScissorMode(imageArea.x, imageArea.y, imageArea.width, imageArea.height);
                DrawTexturePro(textures[selectedFileIndex], sourceRec, destRec, origin, 0.0f, WHITE);
                EndScissorMode();
            } else {
                // If the image failed to load, display an error message
                const char* errorMessage = "Failed to load image";
                int textWidth = MeasureText(errorMessage, fontSize);
                int textX = dropArea.width + (availableWidth - textWidth) / 2;
                int textY = (availableHeight - fontSize) / 2;
                DrawText(errorMessage, textX, textY, fontSize, RED);
                TraceLog(LOG_WARNING, "Failed to load texture: %s", selectedFilename);
            }
        } else {
            // If the selected file is not an image, display the filename
            int filenameWidth = MeasureText(selectedFilename, fontSize);
            int filenameX = dropArea.width + (availableWidth - filenameWidth) / 2;
            int filenameY = (availableHeight - fontSize) / 2;
            DrawText(selectedFilename, filenameX, filenameY, fontSize, DARKGRAY);
        }
    } else {
        // Calculate the available space for the text
        int availableWidth = currentScreenWidth - dropArea.width;
        int availableHeight = currentScreenHeight;

        const char* noFileSelectedText = "No file selected";
        int textWidth = MeasureText(noFileSelectedText, fontSize);
        int textX = dropArea.width + (availableWidth - textWidth) / 2;
        int textY = (availableHeight - fontSize) / 2;
        DrawText(noFileSelectedText, textX, textY, fontSize, GRAY);
    }

    EndDrawing();
}

void unloadAllTextures() {
    for (int i = 0; i < fileCount; i++) {
        if (textures[i].id > 0) {
            UnloadTexture(textures[i]);
            TraceLog(LOG_INFO, "Unloaded texture for %s with ID %d", filenames[i], textures[i].id);
        }
    }
}

int main() {
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 450;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Basic GUI");
    SetTargetFPS(60);
    SetTraceLogLevel(LOG_INFO);

    // Define file drop area
    Rectangle dropArea = { 0, 0, screenWidth / 4, screenHeight };

    // Initialize scrollbar
    ScrollBar scrollBar;
    initScrollBar(&scrollBar, dropArea);

    // Variables for scrollbar visibility and content height
    float totalFilenamesHeight = 0;
    float maxFilenameWidth = 0;
    bool showScrollBar = false;
    bool showHorizontalScrollBar = false;

    // Initialize textures array
    for (int i = 0; i < MAX_FILES; i++) {
        textures[i].id = 0; // Mark as not loaded
    }

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Update
        update(&dropArea, &scrollBar, &totalFilenamesHeight, &maxFilenameWidth, &showScrollBar, &showHorizontalScrollBar);

        // Draw
        draw(dropArea, scrollBar, totalFilenamesHeight, maxFilenameWidth, showScrollBar, showHorizontalScrollBar);
    }

    // De-Initialization
    unloadAllTextures();
    CloseWindow(); // Close window and OpenGL context

    return 0;
}
