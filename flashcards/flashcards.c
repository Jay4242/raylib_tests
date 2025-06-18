#include "raylib.h"
#include <string.h> // Required for strlen, strcpy, strncmp, strchr, strcspn, strncpy
#include <stdlib.h> // Required for rand, srand
#include <time.h>   // Required for time
#include <stdio.h>  // Required for snprintf, FILE operations

// Define game states
typedef enum GameScreen { MENU = 0, SET_SELECTION, QUIZ, EDITOR } GameScreen;

// Max characters for text input fields
#define MAX_TEXT_LENGTH 256

// Max number of flashcards per set
#define MAX_FLASHCARDS_PER_SET 500

// Max number of flashcard sets
#define MAX_SETS 50

// Flashcard structure
typedef struct Flashcard {
    char front[MAX_TEXT_LENGTH];
    char back[MAX_TEXT_LENGTH];
} Flashcard;

// FlashcardSet structure
typedef struct FlashcardSet {
    char name[MAX_TEXT_LENGTH];
    Flashcard cards[MAX_FLASHCARDS_PER_SET];
    int cardCount;
} FlashcardSet;

// Global flashcard set storage
FlashcardSet sets[MAX_SETS];
int setCount = 0;
int currentSetIndex = -1; // Index of the currently active set (-1 for no set selected)
int highlightedSetIndex = -1; // Index of the set currently highlighted in the list

// To remember which screen to go to after set selection
GameScreen nextScreenAfterSetSelection = MENU;

// Function to shuffle an array (Fisher-Yates algorithm)
void ShuffleArray(int *arr, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

// Function to load flashcards from flashcards.cards
// Custom format:
// #SET:Set Name
// Front Text|Back Text
// Front Text 2|Back Text 2
// ...
void LoadFlashcards(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        TraceLog(LOG_WARNING, "%s not found. Starting with empty sets.", fileName);
        return;
    }

    char line[MAX_TEXT_LENGTH * 2]; // Buffer for reading lines
    setCount = 0;
    currentSetIndex = -1; // Reset current set on load
    highlightedSetIndex = -1; // Reset highlighted set on load

    int currentLoadingSetIndex = -1;

    while (fgets(line, sizeof(line), file) != NULL) {
        // Trim newline characters
        line[strcspn(line, "\n")] = 0;

        if (strncmp(line, "#SET:", 5) == 0) {
            if (setCount < MAX_SETS) {
                currentLoadingSetIndex = setCount;
                // Use strncpy for set name to prevent overflow
                strncpy(sets[currentLoadingSetIndex].name, line + 5, MAX_TEXT_LENGTH - 1);
                sets[currentLoadingSetIndex].name[MAX_TEXT_LENGTH - 1] = '\0'; // Ensure null-termination
                sets[currentLoadingSetIndex].cardCount = 0;
                setCount++;
                TraceLog(LOG_INFO, "Loaded Set: %s", sets[currentLoadingSetIndex].name);
            } else {
                TraceLog(LOG_WARNING, "Max sets reached during loading. Skipping set: %s", line + 5);
                currentLoadingSetIndex = -1; // Stop adding cards to this skipped set
            }
        } else if (currentLoadingSetIndex != -1) {
            char *delimiter = strchr(line, '|');
            if (delimiter != NULL) {
                if (sets[currentLoadingSetIndex].cardCount < MAX_FLASHCARDS_PER_SET) {
                    *delimiter = '\0'; // Null-terminate front part

                    // Use strncpy for front text to prevent overflow
                    strncpy(sets[currentLoadingSetIndex].cards[sets[currentLoadingSetIndex].cardCount].front, line, MAX_TEXT_LENGTH - 1);
                    sets[currentLoadingSetIndex].cards[sets[currentLoadingSetIndex].cardCount].front[MAX_TEXT_LENGTH - 1] = '\0'; // Ensure null-termination

                    // Use strncpy for back text to prevent overflow
                    strncpy(sets[currentLoadingSetIndex].cards[sets[currentLoadingSetIndex].cardCount].back, delimiter + 1, MAX_TEXT_LENGTH - 1);
                    sets[currentLoadingSetIndex].cards[sets[currentLoadingSetIndex].cardCount].back[MAX_TEXT_LENGTH - 1] = '\0'; // Ensure null-termination

                    sets[currentLoadingSetIndex].cardCount++;
                } else {
                    TraceLog(LOG_WARNING, "Max cards reached for set '%s' during loading. Skipping card.", sets[currentLoadingSetIndex].name);
                }
            }
        }
    }
    fclose(file);
    TraceLog(LOG_INFO, "Finished loading flashcards. Total sets: %d", setCount);

    // If any sets were loaded, default to the first one
    if (setCount > 0) {
        currentSetIndex = 0;
        highlightedSetIndex = 0; // Also highlight the first set
    }
}

// Function to save flashcards to flashcards.cards
void SaveFlashcards(const char *fileName) {
    FILE *file = fopen(fileName, "w");
    if (file == NULL) {
        TraceLog(LOG_ERROR, "Could not open %s for writing.", fileName);
        return;
    }

    for (int i = 0; i < setCount; i++) {
        fprintf(file, "#SET:%s\n", sets[i].name);
        for (int j = 0; j < sets[i].cardCount; j++) {
            fprintf(file, "%s|%s\n", sets[i].cards[j].front, sets[i].cards[j].back);
        }
    }

    fclose(file);
    TraceLog(LOG_INFO, "Flashcards saved to %s", fileName);
}


int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Flashcards GUI");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

    LoadFlashcards("flashcards.cards"); // Load flashcards at startup

    GameScreen currentScreen = MENU; // Initial game state

    // Define button rectangles for the menu
    Rectangle quizButton = { screenWidth/2 - 100, screenHeight/2 + 30, 200, 50 };
    Rectangle editorButton = { screenWidth/2 - 100, screenHeight/2 + 90, 200, 50 };

    // SET_SELECTION screen variables
    #define SET_SELECTION_TEXTBOX_WIDTH 300
    #define SET_LIST_ITEM_HEIGHT 30
    #define SET_SELECTION_BOTTOM_BUTTON_WIDTH 200 // Increased width for "Create New Set" and other bottom buttons
    #define SET_SELECTION_BOTTOM_BUTTON_HEIGHT 50
    #define SET_SELECTION_BOTTOM_BUTTON_SPACING 20

    char newSetNameText[MAX_TEXT_LENGTH] = "\0";
    int newSetNameTextLength = 0;
    bool newSetNameActive = false;

    Rectangle newSetTextBox = { screenWidth/2 - SET_SELECTION_TEXTBOX_WIDTH/2, 100, SET_SELECTION_TEXTBOX_WIDTH, 40 };
    Rectangle createSetButton = { screenWidth/2 - SET_SELECTION_BOTTOM_BUTTON_WIDTH/2, 160, SET_SELECTION_BOTTOM_BUTTON_WIDTH, 50 };

    // Calculate positions for bottom buttons
    float bottomButtonY = screenHeight - SET_SELECTION_BOTTOM_BUTTON_HEIGHT - 20; // 20px from bottom
    float totalButtonsWidth = 3 * SET_SELECTION_BOTTOM_BUTTON_WIDTH + 2 * SET_SELECTION_BOTTOM_BUTTON_SPACING;
    float startX = (screenWidth - totalButtonsWidth) / 2;

    Rectangle selectSetButton = { startX, bottomButtonY, SET_SELECTION_BOTTOM_BUTTON_WIDTH, SET_SELECTION_BOTTOM_BUTTON_HEIGHT };
    Rectangle deleteSetButton = { startX + SET_SELECTION_BOTTOM_BUTTON_WIDTH + SET_SELECTION_BOTTOM_BUTTON_SPACING, bottomButtonY, SET_SELECTION_BOTTOM_BUTTON_WIDTH, SET_SELECTION_BOTTOM_BUTTON_HEIGHT };
    Rectangle setSelectionBackToMenuButton = { startX + 2 * (SET_SELECTION_BOTTOM_BUTTON_WIDTH + SET_SELECTION_BOTTOM_BUTTON_SPACING), bottomButtonY, SET_SELECTION_BOTTOM_BUTTON_WIDTH, SET_SELECTION_BOTTOM_BUTTON_HEIGHT };

    Rectangle setListRec = { screenWidth/2 - (screenWidth - 80)/2, 230, screenWidth - 80, bottomButtonY - 230 - 20 }; // Space for new buttons at bottom
    float setListScrollOffset = 0.0f;
    const int SET_DISPLAY_FONT_SIZE = 20;


    // Editor screen variables
    // Input fields and buttons will be on the left side
    #define EDITOR_LEFT_PANEL_X 20
    #define EDITOR_LEFT_PANEL_WIDTH 350
    #define EDITOR_TEXTBOX_WIDTH 300
    #define EDITOR_BUTTON_WIDTH 300

    char frontText[MAX_TEXT_LENGTH] = "\0";
    int frontTextLength = 0;
    // Adjusted Y positions for more space and higher placement
    Rectangle frontTextBox = { EDITOR_LEFT_PANEL_X + (EDITOR_LEFT_PANEL_WIDTH - EDITOR_TEXTBOX_WIDTH)/2, screenHeight/2 - 120, EDITOR_TEXTBOX_WIDTH, 40 };
    bool frontBoxActive = false;

    char backText[MAX_TEXT_LENGTH] = "\0";
    int backTextLength = 0;
    // Adjusted Y positions for more space and higher placement
    Rectangle backTextBox = { EDITOR_LEFT_PANEL_X + (EDITOR_LEFT_PANEL_WIDTH - EDITOR_TEXTBOX_WIDTH)/2, screenHeight/2 - 50, EDITOR_TEXTBOX_WIDTH, 40 };
    bool backBoxActive = false;

    // Adjusted Y positions for higher placement
    Rectangle addButton = { EDITOR_LEFT_PANEL_X + (EDITOR_LEFT_PANEL_WIDTH - EDITOR_BUTTON_WIDTH)/2, screenHeight/2 + 20, EDITOR_BUTTON_WIDTH, 50 };
    Rectangle editorBackToMenuButton = { EDITOR_LEFT_PANEL_X + (EDITOR_LEFT_PANEL_WIDTH - EDITOR_BUTTON_WIDTH)/2, screenHeight/2 + 80, EDITOR_BUTTON_WIDTH, 50 };

    // Flashcard list display on the right side
    // Adjusted height to make space for the delete button
    Rectangle flashcardListRec = { EDITOR_LEFT_PANEL_X + EDITOR_LEFT_PANEL_WIDTH + 20, 100, screenWidth - (EDITOR_LEFT_PANEL_X + EDITOR_LEFT_PANEL_WIDTH + 20) - 20, screenHeight - 100 - 20 - 60 }; // -60 for delete button + padding
    float flashcardListScrollOffset = 0.0f;
    const int FLASHCARD_DISPLAY_FONT_SIZE = 18;
    const int FLASHCARD_DISPLAY_LINE_SPACING = 5;

    // New: Selected card for editing/deletion
    int selectedCardIndex = -1; // -1 means no card selected

    // New: Delete button
    Rectangle deleteButton = { flashcardListRec.x + flashcardListRec.width/2 - EDITOR_BUTTON_WIDTH/2, flashcardListRec.y + flashcardListRec.height + 20, EDITOR_BUTTON_WIDTH, 50 };


    // Quiz screen variables
    int currentCardIndex = 0;
    bool showFront = true;
    int quizOrder[MAX_FLASHCARDS_PER_SET]; // Stores randomized indices for the quiz session
    bool quizInitialized = false;  // Flag to ensure quiz setup happens once per session
    bool noCardsMessage = false;   // Flag to show message if no cards exist

    Rectangle revealButton = { screenWidth/2 - 100, screenHeight/2 + 50, 200, 50 };
    Rectangle nextCardButton = { screenWidth/2 - 100, screenHeight/2 + 110, 200, 50 };
    Rectangle quizBackToMenuButton = { screenWidth/2 - 100, screenHeight/2 + 170, 200, 50 };
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        switch(currentScreen)
        {
            case MENU:
            {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    Vector2 mousePoint = GetMousePosition();
                    if (CheckCollisionPointRec(mousePoint, quizButton))
                    {
                        currentScreen = SET_SELECTION;
                        nextScreenAfterSetSelection = QUIZ;
                        // Ensure highlighted set is valid if currentSetIndex is valid
                        if (currentSetIndex != -1 && currentSetIndex < setCount) {
                            highlightedSetIndex = currentSetIndex;
                        } else if (setCount > 0) {
                            highlightedSetIndex = 0; // Default to first set if no current set
                        } else {
                            highlightedSetIndex = -1; // No sets available
                        }
                    }
                    else if (CheckCollisionPointRec(mousePoint, editorButton))
                    {
                        currentScreen = SET_SELECTION;
                        nextScreenAfterSetSelection = EDITOR;
                        // Reset editor state when entering
                        frontText[0] = '\0';
                        frontTextLength = 0;
                        backText[0] = '\0';
                        backTextLength = 0;
                        frontBoxActive = false;
                        backBoxActive = false;
                        selectedCardIndex = -1; // No card selected initially
                        flashcardListScrollOffset = 0.0f;
                        // Ensure highlighted set is valid if currentSetIndex is valid
                        if (currentSetIndex != -1 && currentSetIndex < setCount) {
                            highlightedSetIndex = currentSetIndex;
                        } else if (setCount > 0) {
                            highlightedSetIndex = 0; // Default to first set if no current set
                        } else {
                            highlightedSetIndex = -1; // No sets available
                        }
                    }
                }
            } break;
            case SET_SELECTION:
            {
                Vector2 mousePoint = GetMousePosition();

                // Handle new set name input
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mousePoint, newSetTextBox)) {
                        newSetNameActive = true;
                    } else {
                        newSetNameActive = false;
                    }
                }

                if (newSetNameActive) {
                    int key = GetCharPressed();
                    while (key > 0) {
                        if ((key >= 32) && (key <= 125) && (newSetNameTextLength < MAX_TEXT_LENGTH - 1)) {
                            newSetNameText[newSetNameTextLength] = (char)key;
                            newSetNameTextLength++;
                            newSetNameText[newSetNameTextLength] = '\0';
                        }
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE)) {
                        if (newSetNameTextLength > 0) {
                            newSetNameTextLength--;
                            newSetNameText[newSetNameTextLength] = '\0';
                        }
                    }
                }

                // Handle button clicks
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mousePoint, createSetButton)) {
                        if (newSetNameTextLength > 0 && setCount < MAX_SETS) {
                            // Check if set name already exists
                            bool nameExists = false;
                            for (int i = 0; i < setCount; i++) {
                                if (strcmp(sets[i].name, newSetNameText) == 0) {
                                    nameExists = true;
                                    TraceLog(LOG_WARNING, "Set with name '%s' already exists.", newSetNameText);
                                    break;
                                }
                            }

                            if (!nameExists) {
                                strncpy(sets[setCount].name, newSetNameText, MAX_TEXT_LENGTH - 1);
                                sets[setCount].name[MAX_TEXT_LENGTH - 1] = '\0';
                                sets[setCount].cardCount = 0;
                                currentSetIndex = setCount; // Select the newly created set
                                highlightedSetIndex = setCount; // Also highlight it
                                setCount++;
                                TraceLog(LOG_INFO, "New set created: %s", newSetNameText);
                                newSetNameText[0] = '\0'; // Clear input
                                newSetNameTextLength = 0;
                                newSetNameActive = false;
                                // Do not immediately go to next screen, user might want to create more or select another
                            }
                        } else if (setCount >= MAX_SETS) {
                            TraceLog(LOG_WARNING, "Max sets reached (%d). Cannot create more.", MAX_SETS);
                        } else {
                            TraceLog(LOG_WARNING, "Set name cannot be empty.");
                        }
                    } else if (CheckCollisionPointRec(mousePoint, setSelectionBackToMenuButton)) {
                        currentScreen = MENU;
                    } else if (CheckCollisionPointRec(mousePoint, selectSetButton)) {
                        if (highlightedSetIndex != -1 && highlightedSetIndex < setCount) {
                            currentSetIndex = highlightedSetIndex;
                            TraceLog(LOG_INFO, "Selected set: %s", sets[currentSetIndex].name);
                            currentScreen = nextScreenAfterSetSelection; // Go to next screen
                        } else {
                            TraceLog(LOG_WARNING, "No set highlighted to select.");
                        }
                    } else if (CheckCollisionPointRec(mousePoint, deleteSetButton)) {
                        if (highlightedSetIndex != -1 && highlightedSetIndex < setCount) {
                            TraceLog(LOG_INFO, "Deleting set: %s", sets[highlightedSetIndex].name);
                            // Shift sets
                            for (int j = highlightedSetIndex; j < setCount - 1; j++) {
                                sets[j] = sets[j+1];
                            }
                            setCount--;
                            // Adjust currentSetIndex if the deleted set was the current one or before it
                            if (currentSetIndex == highlightedSetIndex) {
                                currentSetIndex = -1; // No set selected
                            } else if (currentSetIndex > highlightedSetIndex) {
                                currentSetIndex--; // Shift index if a set before it was deleted
                            }
                            // Adjust highlightedSetIndex
                            if (setCount == 0) {
                                highlightedSetIndex = -1; // No sets left
                            } else if (highlightedSetIndex >= setCount) {
                                highlightedSetIndex = setCount - 1; // If last set deleted, highlight new last
                            }
                            // Adjust scroll offset if content height changes
                            float itemHeight = SET_DISPLAY_FONT_SIZE + 10;
                            float totalContentHeight = setCount * itemHeight;
                            float maxScrollOffset = totalContentHeight - setListRec.height;
                            if (maxScrollOffset < 0) maxScrollOffset = 0;
                            if (setListScrollOffset > maxScrollOffset) setListScrollOffset = maxScrollOffset;
                        } else {
                            TraceLog(LOG_WARNING, "No set highlighted to delete.");
                        }
                    } else if (CheckCollisionPointRec(mousePoint, setListRec)) {
                        // Check for set highlighting
                        float itemHeight = SET_DISPLAY_FONT_SIZE + 10; // Height of each set item + padding
                        int clickedItem = -1;
                        for (int i = 0; i < setCount; i++) {
                            float itemY = setListRec.y + (i * itemHeight) - setListScrollOffset;
                            Rectangle itemClickRec = { setListRec.x, itemY, setListRec.width, itemHeight };

                            if (CheckCollisionPointRec(mousePoint, itemClickRec)) {
                                clickedItem = i;
                                break;
                            }
                        }
                        highlightedSetIndex = clickedItem; // Update highlighted set
                    }
                }

                // Handle scrolling for the set list
                if (CheckCollisionPointRec(mousePoint, setListRec)) {
                    float wheelMove = GetMouseWheelMove();
                    if (wheelMove != 0) {
                        float itemHeight = SET_DISPLAY_FONT_SIZE + 10;
                        float totalContentHeight = setCount * itemHeight;
                        float maxScrollOffset = totalContentHeight - setListRec.height;
                        if (maxScrollOffset < 0) maxScrollOffset = 0;

                        setListScrollOffset -= wheelMove * 20; // Adjust scroll speed
                        if (setListScrollOffset < 0) setListScrollOffset = 0;
                        if (setListScrollOffset > maxScrollOffset) setListScrollOffset = maxScrollOffset;
                    }
                }

                if (IsKeyPressed(KEY_ESCAPE)) {
                    currentScreen = MENU;
                }
            } break;
            case QUIZ:
            {
                // Check if a set is selected and has cards
                bool canQuiz = (currentSetIndex != -1 && sets[currentSetIndex].cardCount > 0);

                // Initialize quiz state only once when entering the screen
                if (!quizInitialized)
                {
                    if (!canQuiz)
                    {
                        noCardsMessage = true;
                    }
                    else
                    {
                        noCardsMessage = false;
                        // Populate quizOrder with indices
                        for (int i = 0; i < sets[currentSetIndex].cardCount; i++)
                        {
                            quizOrder[i] = i;
                        }
                        // Seed random number generator and shuffle
                        srand((unsigned int)time(NULL));
                        ShuffleArray(quizOrder, sets[currentSetIndex].cardCount);
                        currentCardIndex = 0;
                        showFront = true;
                    }
                    quizInitialized = true;
                }

                Vector2 mousePoint = GetMousePosition();

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    if (CheckCollisionPointRec(mousePoint, revealButton) && canQuiz)
                    {
                        showFront = !showFront; // Toggle front/back
                    }
                    else if (CheckCollisionPointRec(mousePoint, nextCardButton) && canQuiz)
                    {
                        currentCardIndex = (currentCardIndex + 1) % sets[currentSetIndex].cardCount; // Move to next card, loop if at end
                        showFront = true; // Always show front of new card
                    }
                    else if (CheckCollisionPointRec(mousePoint, quizBackToMenuButton))
                    {
                        currentScreen = MENU;
                        quizInitialized = false; // Reset for next quiz session
                    }
                }

                if (IsKeyPressed(KEY_ESCAPE)) // Allow returning to menu
                {
                    currentScreen = MENU;
                    quizInitialized = false; // Reset for next quiz session
                }
            } break;
            case EDITOR:
            {
                Vector2 mousePoint = GetMousePosition();

                // Handle text box activation
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    if (CheckCollisionPointRec(mousePoint, frontTextBox))
                    {
                        frontBoxActive = true;
                        backBoxActive = false;
                    }
                    else if (CheckCollisionPointRec(mousePoint, backTextBox))
                    {
                        backBoxActive = true;
                        frontBoxActive = false;
                    }
                    else // Clicked outside text boxes
                    {
                        frontBoxActive = false;
                        backBoxActive = false;
                    }

                    // Check for button clicks
                    if (CheckCollisionPointRec(mousePoint, addButton) && currentSetIndex != -1)
                    {
                        if (strlen(frontText) > 0 && strlen(backText) > 0) {
                            if (selectedCardIndex != -1) {
                                // Update existing flashcard
                                strncpy(sets[currentSetIndex].cards[selectedCardIndex].front, frontText, MAX_TEXT_LENGTH - 1);
                                sets[currentSetIndex].cards[selectedCardIndex].front[MAX_TEXT_LENGTH - 1] = '\0';
                                strncpy(sets[currentSetIndex].cards[selectedCardIndex].back, backText, MAX_TEXT_LENGTH - 1);
                                sets[currentSetIndex].cards[selectedCardIndex].back[MAX_TEXT_LENGTH - 1] = '\0';
                                TraceLog(LOG_INFO, "Flashcard Updated: Index %d, Front='%s', Back='%s'", selectedCardIndex, frontText, backText);
                            } else if (sets[currentSetIndex].cardCount < MAX_FLASHCARDS_PER_SET) {
                                // Add new flashcard
                                strncpy(sets[currentSetIndex].cards[sets[currentSetIndex].cardCount].front, frontText, MAX_TEXT_LENGTH - 1);
                                sets[currentSetIndex].cards[sets[currentSetIndex].cardCount].front[MAX_TEXT_LENGTH - 1] = '\0';
                                strncpy(sets[currentSetIndex].cards[sets[currentSetIndex].cardCount].back, backText, MAX_TEXT_LENGTH - 1);
                                sets[currentSetIndex].cards[sets[currentSetIndex].cardCount].back[MAX_TEXT_LENGTH - 1] = '\0';
                                sets[currentSetIndex].cardCount++;
                                TraceLog(LOG_INFO, "Flashcard Added (%d/%d): Front='%s', Back='%s'", sets[currentSetIndex].cardCount, MAX_FLASHCARDS_PER_SET, frontText, backText);
                            } else {
                                TraceLog(LOG_WARNING, "Max flashcards reached for set '%s' (%d). Cannot add more.", sets[currentSetIndex].name, MAX_FLASHCARDS_PER_SET);
                            }

                            // Clear text fields and reset selection after add/update
                            frontText[0] = '\0';
                            frontTextLength = 0;
                            backText[0] = '\0';
                            backTextLength = 0;
                            frontBoxActive = false;
                            backBoxActive = false;
                            selectedCardIndex = -1; // Deselect card

                            // Scroll to the bottom to show the newly added/updated card if it's new
                            if (selectedCardIndex == -1) { // Only scroll if a new card was added
                                float cardHeight = FLASHCARD_DISPLAY_FONT_SIZE * 2 + FLASHCARD_DISPLAY_LINE_SPACING;
                                float totalContentHeight = sets[currentSetIndex].cardCount * cardHeight;
                                float maxScrollOffset = totalContentHeight - flashcardListRec.height;
                                if (maxScrollOffset < 0) maxScrollOffset = 0;
                                flashcardListScrollOffset = maxScrollOffset;
                            }
                        } else {
                            TraceLog(LOG_WARNING, "Cannot add/update empty flashcard. Both front and back must have text.");
                        }
                    }
                    else if (CheckCollisionPointRec(mousePoint, editorBackToMenuButton))
                    {
                        currentScreen = MENU;
                        // Clear text fields and reset state when leaving editor
                        frontText[0] = '\0';
                        frontTextLength = 0;
                        backText[0] = '\0';
                        backTextLength = 0;
                        frontBoxActive = false;
                        backBoxActive = false;
                        selectedCardIndex = -1;
                        flashcardListScrollOffset = 0.0f; // Reset scroll when leaving editor
                    }
                    else if (CheckCollisionPointRec(mousePoint, deleteButton) && selectedCardIndex != -1 && currentSetIndex != -1)
                    {
                        // Delete selected flashcard
                        if (selectedCardIndex >= 0 && selectedCardIndex < sets[currentSetIndex].cardCount)
                        {
                            TraceLog(LOG_INFO, "Deleting flashcard at index %d from set '%s': Front='%s'", selectedCardIndex, sets[currentSetIndex].name, sets[currentSetIndex].cards[selectedCardIndex].front);
                            for (int i = selectedCardIndex; i < sets[currentSetIndex].cardCount - 1; i++)
                            {
                                sets[currentSetIndex].cards[i] = sets[currentSetIndex].cards[i+1]; // Shift elements left
                            }
                            sets[currentSetIndex].cardCount--;
                            // Clear text fields and reset selection after deletion
                            frontText[0] = '\0';
                            frontTextLength = 0;
                            backText[0] = '\0';
                            backTextLength = 0;
                            frontBoxActive = false;
                            backBoxActive = false;
                            selectedCardIndex = -1; // Deselect card

                            // Adjust scroll offset if content height changes
                            float cardHeight = FLASHCARD_DISPLAY_FONT_SIZE * 2 + FLASHCARD_DISPLAY_LINE_SPACING;
                            float totalContentHeight = sets[currentSetIndex].cardCount * cardHeight;
                            float maxScrollOffset = totalContentHeight - flashcardListRec.height;
                            if (maxScrollOffset < 0) maxScrollOffset = 0;
                            if (flashcardListScrollOffset > maxScrollOffset) flashcardListScrollOffset = maxScrollOffset;
                        }
                    }
                    else if (CheckCollisionPointRec(mousePoint, flashcardListRec) && currentSetIndex != -1) // Check clicks within the list area for card selection
                    {
                        float cardHeight = FLASHCARD_DISPLAY_FONT_SIZE * 2 + FLASHCARD_DISPLAY_LINE_SPACING;
                        int clickedCard = -1;
                        for (int i = 0; i < sets[currentSetIndex].cardCount; i++)
                        {
                            float cardY = flashcardListRec.y + (i * cardHeight) - flashcardListScrollOffset;
                            Rectangle cardClickRec = { flashcardListRec.x, cardY, flashcardListRec.width, cardHeight };

                            if (CheckCollisionPointRec(mousePoint, cardClickRec))
                            {
                                clickedCard = i;
                                break;
                            }
                        }

                        if (clickedCard != -1)
                        {
                            selectedCardIndex = clickedCard;
                            strncpy(frontText, sets[currentSetIndex].cards[selectedCardIndex].front, MAX_TEXT_LENGTH - 1);
                            frontText[MAX_TEXT_LENGTH - 1] = '\0';
                            frontTextLength = strlen(frontText);
                            strncpy(backText, sets[currentSetIndex].cards[selectedCardIndex].back, MAX_TEXT_LENGTH - 1);
                            backText[MAX_TEXT_LENGTH - 1] = '\0';
                            backTextLength = strlen(backText);
                            frontBoxActive = true; // Activate front box for editing
                            backBoxActive = false;
                        } else {
                            // If clicked inside the list area but not on a card, deselect
                            selectedCardIndex = -1;
                            frontText[0] = '\0';
                            frontTextLength = 0;
                            backText[0] = '\0';
                            backTextLength = 0;
                            frontBoxActive = false;
                            backBoxActive = false;
                        }
                    }
                }

                // Handle Tab key for switching between text boxes
                if (IsKeyPressed(KEY_TAB)) {
                    if (frontBoxActive) {
                        frontBoxActive = false;
                        backBoxActive = true;
                    } else if (backBoxActive) {
                        backBoxActive = false;
                        frontBoxActive = true;
                    } else { // If neither is active, activate front box
                        frontBoxActive = true;
                    }
                }

                // Handle text input for active box
                if (frontBoxActive)
                {
                    int key = GetCharPressed();
                    while (key > 0)
                    {
                        // Allow printable ASCII characters (32 to 125)
                        if ((key >= 32) && (key <= 125) && (frontTextLength < MAX_TEXT_LENGTH - 1))
                        {
                            frontText[frontTextLength] = (char)key;
                            frontTextLength++;
                            frontText[frontTextLength] = '\0'; // Null-terminate the string
                        }
                        key = GetCharPressed(); // Get next character in queue
                    }

                    if (IsKeyPressed(KEY_BACKSPACE))
                    {
                        if (frontTextLength > 0)
                        {
                            frontTextLength--;
                            frontText[frontTextLength] = '\0'; // Null-terminate the string
                        }
                    }
                }
                else if (backBoxActive)
                {
                    int key = GetCharPressed();
                    while (key > 0)
                    {
                        // Allow printable ASCII characters (32 to 125)
                        if ((key >= 32) && (key <= 125) && (backTextLength < MAX_TEXT_LENGTH - 1))
                        {
                            backText[backTextLength] = (char)key;
                            backTextLength++;
                            backText[backTextLength] = '\0'; // Null-terminate the string
                        }
                        key = GetCharPressed(); // Get next character in queue
                    }

                    if (IsKeyPressed(KEY_BACKSPACE))
                    {
                        if (backTextLength > 0)
                        {
                            backTextLength--;
                            backText[backTextLength] = '\0'; // Null-terminate the string
                        }
                    }
                }

                // Handle scrolling for the flashcard list
                if (CheckCollisionPointRec(mousePoint, flashcardListRec)) {
                    float wheelMove = GetMouseWheelMove();
                    if (wheelMove != 0) {
                        float cardHeight = FLASHCARD_DISPLAY_FONT_SIZE * 2 + FLASHCARD_DISPLAY_LINE_SPACING;
                        float totalContentHeight = (currentSetIndex != -1) ? sets[currentSetIndex].cardCount * cardHeight : 0;
                        float maxScrollOffset = totalContentHeight - flashcardListRec.height;
                        if (maxScrollOffset < 0) maxScrollOffset = 0; // No scrolling needed if content fits

                        flashcardListScrollOffset -= wheelMove * 20; // Adjust scroll speed
                        if (flashcardListScrollOffset < 0) flashcardListScrollOffset = 0;
                        if (flashcardListScrollOffset > maxScrollOffset) flashcardListScrollOffset = maxScrollOffset;
                    }
                }

                // Allow returning to menu with ESC, but also clear fields
                if (IsKeyPressed(KEY_ESCAPE))
                {
                    currentScreen = MENU;
                    frontText[0] = '\0';
                    frontTextLength = 0;
                    backText[0] = '\0';
                    backTextLength = 0;
                    frontBoxActive = false;
                    backBoxActive = false;
                    selectedCardIndex = -1;
                    flashcardListScrollOffset = 0.0f; // Reset scroll when leaving editor
                }
            } break;
            default: break;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            switch(currentScreen)
            {
                case MENU:
                {
                    // Draw main title
                    DrawText("Welcome to Flashcards!", screenWidth/2 - MeasureText("Welcome to Flashcards!", 40)/2, screenHeight/2 - 80, 40, DARKGRAY);

                    // Draw "Start Quiz" Button
                    DrawRectangleRec(quizButton, LIGHTGRAY);
                    DrawText("Start Quiz", quizButton.x + quizButton.width/2 - MeasureText("Start Quiz", 20)/2, quizButton.y + quizButton.height/2 - 10, 20, DARKBLUE);
                    DrawRectangleLinesEx(quizButton, 2, DARKGRAY); // Add border

                    // Draw "Flashcard Editor" Button
                    DrawRectangleRec(editorButton, LIGHTGRAY);
                    DrawText("Flashcard Editor", editorButton.x + editorButton.width/2 - MeasureText("Flashcard Editor", 20)/2, editorButton.y + editorButton.height/2 - 10, 20, DARKBLUE);
                    DrawRectangleLinesEx(editorButton, 2, DARKGRAY); // Add border
                } break;
                case SET_SELECTION:
                {
                    DrawText("Select or Create a Flashcard Set", screenWidth/2 - MeasureText("Select or Create a Flashcard Set", 30)/2, 20, 30, DARKGRAY);

                    // New Set Name Input
                    DrawText("New Set Name:", newSetTextBox.x, newSetTextBox.y - 25, 20, DARKGRAY);
                    DrawRectangleRec(newSetTextBox, WHITE);
                    DrawRectangleLinesEx(newSetTextBox, 2, (newSetNameActive) ? DARKBLUE : DARKGRAY);
                    DrawText(newSetNameText, newSetTextBox.x + 5, newSetTextBox.y + newSetTextBox.height/2 - 10, 20, BLACK);
                    if (newSetNameActive && ((int)(GetTime() * 2.0f) % 2 == 0)) DrawText("_", newSetTextBox.x + 5 + MeasureText(newSetNameText, 20), newSetTextBox.y + newSetTextBox.height/2 - 10, 20, BLACK);

                    // Create Set Button
                    DrawRectangleRec(createSetButton, GREEN);
                    DrawText("Create New Set", createSetButton.x + createSetButton.width/2 - MeasureText("Create New Set", 20)/2, createSetButton.y + createSetButton.height/2 - 10, 20, WHITE);
                    DrawRectangleLinesEx(createSetButton, 2, DARKGREEN);

                    // Existing Sets List
                    DrawText("Existing Sets:", setListRec.x, setListRec.y - 25, 20, DARKGRAY);
                    DrawRectangleRec(setListRec, LIGHTGRAY);
                    DrawRectangleLinesEx(setListRec, 2, DARKGRAY);

                    BeginScissorMode(setListRec.x, setListRec.y, setListRec.width, setListRec.height);

                    float itemHeight = SET_DISPLAY_FONT_SIZE + 10;
                    float totalContentHeight = setCount * itemHeight;
                    float maxScrollOffset = totalContentHeight - setListRec.height;
                    if (maxScrollOffset < 0) maxScrollOffset = 0;

                    if (setListScrollOffset < 0) setListScrollOffset = 0;
                    if (setListScrollOffset > maxScrollOffset) setListScrollOffset = maxScrollOffset;

                    for (int i = 0; i < setCount; i++) {
                        float itemY = setListRec.y + (i * itemHeight) - setListScrollOffset;
                        Rectangle itemDisplayRec = { setListRec.x, itemY, setListRec.width, itemHeight };

                        // Only draw if the item is at least partially visible
                        if (itemY < setListRec.y + setListRec.height && itemY + itemHeight > setListRec.y) {
                            // Highlight current selected set
                            if (i == highlightedSetIndex) {
                                DrawRectangleRec(itemDisplayRec, SKYBLUE);
                                DrawRectangleLinesEx(itemDisplayRec, 2, BLUE);
                            }
                            // Removed the else if (i == currentSetIndex) block that drew the gray highlight
                            
                            // Draw set name
                            DrawText(sets[i].name, setListRec.x + 15, (int)itemY + 10, SET_DISPLAY_FONT_SIZE, BLACK);
                        }
                    }
                    EndScissorMode();

                    // Draw "Select Set" Button
                    bool canSelect = (highlightedSetIndex != -1 && highlightedSetIndex < setCount);
                    Color selectBtnColor = canSelect ? DARKBLUE : GRAY;
                    Color selectBtnBorderColor = canSelect ? BLUE : DARKGRAY;
                    DrawRectangleRec(selectSetButton, selectBtnColor);
                    DrawText("Select Set", selectSetButton.x + selectSetButton.width/2 - MeasureText("Select Set", 20)/2, selectSetButton.y + selectSetButton.height/2 - 10, 20, WHITE);
                    DrawRectangleLinesEx(selectSetButton, 2, selectBtnBorderColor);

                    // Draw "Delete Set" Button
                    bool canDelete = (highlightedSetIndex != -1 && highlightedSetIndex < setCount);
                    Color deleteBtnColor = canDelete ? MAROON : GRAY;
                    Color deleteBtnBorderColor = canDelete ? RED : DARKGRAY;
                    DrawRectangleRec(deleteSetButton, deleteBtnColor);
                    DrawText("Delete Set", deleteSetButton.x + deleteSetButton.width/2 - MeasureText("Delete Set", 20)/2, deleteSetButton.y + deleteSetButton.height/2 - 10, 20, WHITE);
                    DrawRectangleLinesEx(deleteSetButton, 2, deleteBtnBorderColor);

                    // Back to Menu Button
                    DrawRectangleRec(setSelectionBackToMenuButton, RED);
                    DrawText("Back to Menu", setSelectionBackToMenuButton.x + setSelectionBackToMenuButton.width/2 - MeasureText("Back to Menu", 20)/2, setSelectionBackToMenuButton.y + setSelectionBackToMenuButton.height/2 - 10, 20, WHITE);
                    DrawRectangleLinesEx(setSelectionBackToMenuButton, 2, MAROON);
                } break;
                case QUIZ:
                {
                    if (currentSetIndex == -1) {
                        DrawText("No set selected!", screenWidth/2 - MeasureText("No set selected!", 20)/2, screenHeight/2 - 50, 20, DARKGRAY);
                        DrawText("Please select a set from the menu.", screenWidth/2 - MeasureText("Please select a set from the menu.", 20)/2, screenHeight/2 - 20, 20, DARKGRAY);
                    } else {
                        DrawText(TextFormat("Quiz Set: %s", sets[currentSetIndex].name), screenWidth/2 - MeasureText(TextFormat("Quiz Set: %s", sets[currentSetIndex].name), 25)/2, 20, 25, DARKGRAY);

                        if (noCardsMessage)
                        {
                            DrawText("No flashcards in this set yet!", screenWidth/2 - MeasureText("No flashcards in this set yet!", 20)/2, screenHeight/2 - 50, 20, DARKGRAY);
                            DrawText("Go to Editor to add some.", screenWidth/2 - MeasureText("Go to Editor to add some.", 20)/2, screenHeight/2 - 20, 20, DARKGRAY);
                        }
                        else
                        {
                            // Display current flashcard
                            const char* currentText = showFront ? sets[currentSetIndex].cards[quizOrder[currentCardIndex]].front : sets[currentSetIndex].cards[quizOrder[currentCardIndex]].back;
                            DrawText(currentText, screenWidth/2 - MeasureText(currentText, 30)/2, screenHeight/2 - 80, 30, DARKGRAY);

                            // Draw "Reveal/Hide" Button
                            DrawRectangleRec(revealButton, SKYBLUE);
                            DrawText(showFront ? "Reveal Back" : "Hide Back", revealButton.x + revealButton.width/2 - MeasureText(showFront ? "Reveal Back" : "Hide Back", 20)/2, revealButton.y + revealButton.height/2 - 10, 20, DARKBLUE);
                            DrawRectangleLinesEx(revealButton, 2, BLUE);

                            // Draw "Next Card" Button
                            DrawRectangleRec(nextCardButton, LIME);
                            DrawText("Next Card", nextCardButton.x + nextCardButton.width/2 - MeasureText("Next Card", 20)/2, nextCardButton.y + nextCardButton.height/2 - 10, 20, DARKGREEN);
                            DrawRectangleLinesEx(nextCardButton, 2, DARKGREEN);
                        }
                    }
                    // Draw "Back to Menu" Button
                    DrawRectangleRec(quizBackToMenuButton, RED);
                    DrawText("Back to Menu", quizBackToMenuButton.x + quizBackToMenuButton.width/2 - MeasureText("Back to Menu", 20)/2, quizBackToMenuButton.y + quizBackToMenuButton.height/2 - 10, 20, WHITE);
                    DrawRectangleLinesEx(quizBackToMenuButton, 2, MAROON);
                } break;
                case EDITOR:
                {
                    DrawText("Flashcard Editor", screenWidth/2 - MeasureText("Flashcard Editor", 30)/2, 20, 30, DARKGRAY);

                    if (currentSetIndex == -1) {
                        DrawText("No set selected!", screenWidth/2 - MeasureText("No set selected!", 20)/2, screenHeight/2 - 50, 20, DARKGRAY);
                        DrawText("Please select a set from the menu.", screenWidth/2 - MeasureText("Please select a set from the menu.", 20)/2, screenHeight/2 - 20, 20, DARKGRAY);
                        // Disable input fields and buttons
                        DrawRectangleRec(frontTextBox, LIGHTGRAY);
                        DrawRectangleRec(backTextBox, LIGHTGRAY);
                        DrawRectangleRec(addButton, LIGHTGRAY);
                        DrawRectangleRec(deleteButton, LIGHTGRAY);
                    } else {
                        DrawText(TextFormat("Editing Set: %s", sets[currentSetIndex].name), EDITOR_LEFT_PANEL_X, 60, 20, DARKGRAY);

                        // Draw Front Text Box
                        DrawText("Front:", frontTextBox.x, frontTextBox.y - 25, 20, DARKGRAY);
                        DrawRectangleRec(frontTextBox, WHITE);
                        DrawRectangleLinesEx(frontTextBox, 2, (frontBoxActive) ? DARKBLUE : DARKGRAY);
                        DrawText(frontText, frontTextBox.x + 5, frontTextBox.y + frontTextBox.height/2 - 10, 20, BLACK);
                        // Draw blinking cursor
                        if (frontBoxActive && ((int)(GetTime() * 2.0f) % 2 == 0)) DrawText("_", frontTextBox.x + 5 + MeasureText(frontText, 20), frontTextBox.y + frontTextBox.height/2 - 10, 20, BLACK);

                        // Draw Back Text Box
                        DrawText("Back:", backTextBox.x, backTextBox.y - 25, 20, DARKGRAY);
                        DrawRectangleRec(backTextBox, WHITE);
                        DrawRectangleLinesEx(backTextBox, 2, (backBoxActive) ? DARKBLUE : DARKGRAY);
                        DrawText(backText, backTextBox.x + 5, backTextBox.y + backTextBox.height/2 - 10, 20, BLACK);
                        // Draw blinking cursor
                        if (backBoxActive && ((int)(GetTime() * 2.0f) % 2 == 0)) DrawText("_", backTextBox.x + 5 + MeasureText(backText, 20), backTextBox.y + backTextBox.height/2 - 10, 20, BLACK);

                        // Draw "Add/Update Flashcard" Button
                        Color addButtonColor = (selectedCardIndex != -1) ? ORANGE : GREEN;
                        Color addButtonBorderColor = (selectedCardIndex != -1) ? GOLD : DARKGREEN;
                        const char* addButtonText = (selectedCardIndex != -1) ? "Update Flashcard" : "Add Flashcard";
                        DrawRectangleRec(addButton, addButtonColor);
                        DrawText(addButtonText, addButton.x + addButton.width/2 - MeasureText(addButtonText, 20)/2, addButton.y + addButton.height/2 - 10, 20, WHITE);
                        DrawRectangleLinesEx(addButton, 2, addButtonBorderColor);

                        // Draw flashcard list background and border
                        DrawRectangleRec(flashcardListRec, LIGHTGRAY);
                        DrawRectangleLinesEx(flashcardListRec, 2, DARKGRAY);
                        DrawText("Existing Flashcards:", flashcardListRec.x, flashcardListRec.y - 25, 20, DARKGRAY);

                        // Use scissor mode to clip drawing to the list area
                        BeginScissorMode(flashcardListRec.x, flashcardListRec.y, flashcardListRec.width, flashcardListRec.height);

                        float cardHeight = FLASHCARD_DISPLAY_FONT_SIZE * 2 + FLASHCARD_DISPLAY_LINE_SPACING;
                        float totalContentHeight = sets[currentSetIndex].cardCount * cardHeight;
                        float maxScrollOffset = totalContentHeight - flashcardListRec.height;
                        if (maxScrollOffset < 0) maxScrollOffset = 0;

                        // Clamp scroll offset (this is also done in update, but good to ensure here too)
                        if (flashcardListScrollOffset < 0) flashcardListScrollOffset = 0;
                        if (flashcardListScrollOffset > maxScrollOffset) flashcardListScrollOffset = maxScrollOffset;

                        for (int i = 0; i < sets[currentSetIndex].cardCount; i++)
                        {
                            float cardY = flashcardListRec.y + (i * cardHeight) - flashcardListScrollOffset;
                            Rectangle cardDisplayRec = { flashcardListRec.x, cardY, flashcardListRec.width, cardHeight };

                            // Only draw if the card is at least partially visible
                            if (cardY < flashcardListRec.y + flashcardListRec.height && cardY + cardHeight > flashcardListRec.y)
                            {
                                // Highlight selected card
                                if (i == selectedCardIndex)
                                {
                                    DrawRectangleRec(cardDisplayRec, SKYBLUE);
                                    DrawRectangleLinesEx(cardDisplayRec, 2, BLUE);
                                }

                                char displayFront[MAX_TEXT_LENGTH + 10];
                                char displayBack[MAX_TEXT_LENGTH + 10];
                                snprintf(displayFront, sizeof(displayFront), "Front: %s", sets[currentSetIndex].cards[i].front);
                                snprintf(displayBack, sizeof(displayBack), "Back: %s", sets[currentSetIndex].cards[i].back);

                                DrawText(displayFront, flashcardListRec.x + 5, (int)cardY, FLASHCARD_DISPLAY_FONT_SIZE, BLACK);
                                DrawText(displayBack, flashcardListRec.x + 5, (int)(cardY + FLASHCARD_DISPLAY_FONT_SIZE), FLASHCARD_DISPLAY_FONT_SIZE, DARKGRAY);
                            }
                        }

                        EndScissorMode();

                        // Draw "Delete Flashcard" Button (only if a card is selected)
                        if (selectedCardIndex != -1)
                        {
                            DrawRectangleRec(deleteButton, MAROON);
                            DrawText("Delete Flashcard", deleteButton.x + deleteButton.width/2 - MeasureText("Delete Flashcard", 20)/2, deleteButton.y + deleteButton.height/2 - 10, 20, WHITE);
                            DrawRectangleLinesEx(deleteButton, 2, DARKGRAY);
                        } else {
                            DrawRectangleRec(deleteButton, LIGHTGRAY); // Draw disabled delete button
                            DrawText("Delete Flashcard", deleteButton.x + deleteButton.width/2 - MeasureText("Delete Flashcard", 20)/2, deleteButton.y + deleteButton.height/2 - 10, 20, GRAY);
                            DrawRectangleLinesEx(deleteButton, 2, DARKGRAY);
                        }
                    }

                    // Draw "Back to Menu" Button
                    DrawRectangleRec(editorBackToMenuButton, RED);
                    DrawText("Back to Menu", editorBackToMenuButton.x + editorBackToMenuButton.width/2 - MeasureText("Back to Menu", 20)/2, editorBackToMenuButton.y + editorBackToMenuButton.height/2 - 10, 20, WHITE);
                    DrawRectangleLinesEx(editorBackToMenuButton, 2, MAROON);
                } break;
                default: break;
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    SaveFlashcards("flashcards.cards"); // Save flashcards before closing
    CloseWindow();        // Close window and unload OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
