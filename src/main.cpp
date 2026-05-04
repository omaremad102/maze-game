#include "Maze.h"
#include "Solver.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr float kCellSize = 4.0f;
constexpr float kWallThickness = 0.25f;
constexpr float kWallHeight = 3.5f;
constexpr float kWalkSpeed = 3.0f;
constexpr float kSprintMultiplier = 1.28f;
constexpr float kEnemyMoveInterval = 0.78f;
constexpr float kPlayerRadius = 0.38f;
constexpr float kJumpDuration = 0.82f;
constexpr float kJumpHeight = 1.05f;
constexpr float kMouseSensitivity = 0.0023f;
constexpr float kCoinSpinSpeed = 85.0f;
constexpr float kAmbientPulse = 0.35f;
constexpr int kHudWidth = 560;
constexpr int kDRow[4] = {-1, 0, 1, 0};
constexpr int kDCol[4] = {0, 1, 0, -1};

enum class Difficulty { Easy, Medium, Hard };
enum class GameScreenState { Menu, Playing, Paused, Won, Lost };

struct DifficultySettings {
    MazeConfig config;
    float timeLimit = 120.0f;
    int hintCharges = 5;
    int scoreBonus = 0;
    bool useTraps = false;
    bool enemyEnabled = false;
    const char* label = "";
};

struct PlayerState {
    std::pair<int, int> cell{0, 0};
    std::pair<int, int> fromCell{0, 0};
    std::pair<int, int> toCell{0, 0};
    Vector2 position{0.0f, 0.0f};
    bool moving = false;
    float moveTimer = 0.0f;
    bool jumping = false;
    float jumpTimer = 0.0f;
    int keys = 0;
    int coins = 0;
    int hintsUsed = 0;
    float yaw = 0.0f;
    float pitch = -0.25f;
};

struct EnemyState {
    std::pair<int, int> cell{0, 0};
    float moveTimer = 0.0f;
    bool active = false;
};


struct GameAssets {
    Sound coin{};
    Sound key{};
    Sound trap{};
    Sound hint{};
    Sound win{};
    Sound lose{};
    Sound enemy{};
    Sound step{};
    bool audioReady = false;
};

struct GameVisuals {
    Texture2D ground{};
    Texture2D wall{};
    Texture2D door{};
    Model groundModel{};
    Model wallModel{};
    Model doorModel{};
    bool ready = false;
};

uint32_t HashPixel(int x, int y, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + static_cast<uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

Color Blend(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)
    };
}

Color Jitter(Color color, int x, int y, int amount, int seed) {
    int delta = static_cast<int>(HashPixel(x, y, seed) % (amount * 2 + 1)) - amount;
    return {
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.r) + delta, 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.g) + delta, 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.b) + delta, 0, 255)),
        color.a
    };
}

Image CreateGroundImage() {
    Image image = GenImageColor(128, 128, Color{88, 104, 68, 255});
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            uint32_t noise = HashPixel(x, y, 11);
            float grassPatch = static_cast<float>((HashPixel(x / 12, y / 12, 17) & 255u)) / 255.0f;
            Color dirt = Color{112, 92, 64, 255};
            Color grass = Color{54, 126, 58, 255};
            Color moss = Color{76, 136, 70, 255};
            Color color = Blend(dirt, grassPatch > 0.58f ? grass : moss, grassPatch > 0.58f ? 0.58f : 0.22f);
            if ((noise % 97u) < 3u) color = Blend(color, Color{150, 140, 116, 255}, 0.45f);
            if ((noise % 43u) < 5u) color = Blend(color, Color{42, 102, 42, 255}, 0.35f);
            ImageDrawPixel(&image, x, y, Jitter(color, x, y, 18, 23));
        }
    }
    return image;
}

Image CreateBrickImage() {
    Image image = GenImageColor(256, 256, Color{96, 72, 62, 255});
    const int brickW = 64;
    const int brickH = 32;
    const int mortar = 4;
    for (int y = 0; y < image.height; ++y) {
        int row = y / brickH;
        int offset = (row % 2) * (brickW / 2);
        for (int x = 0; x < image.width; ++x) {
            int localX = (x + offset) % brickW;
            int localY = y % brickH;
            bool isMortar = localX < mortar || localY < mortar || localY > brickH - mortar;
            if (isMortar) {
                ImageDrawPixel(&image, x, y, Jitter(Color{84, 78, 72, 255}, x, y, 10, 31));
            } else {
                float shade = (localY < 7) ? 0.18f : ((localY > brickH - 8) ? -0.16f : 0.0f);
                Color brick = ((row + (x / brickW)) % 3 == 0) ? Color{150, 76, 55, 255} : Color{126, 65, 50, 255};
                if ((HashPixel(x / 8, y / 8, 41) % 5u) == 0u) brick = Blend(brick, Color{180, 98, 66, 255}, 0.35f);
                ImageDrawPixel(&image, x, y, ColorBrightness(Jitter(brick, x, y, 15, 43), shade));
            }
        }
    }
    return image;
}

Image CreateDoorImage() {
    Image image = GenImageColor(128, 192, Color{86, 48, 28, 255});
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int plank = x / 24;
            Color wood = (plank % 2 == 0) ? Color{122, 72, 38, 255} : Color{96, 56, 32, 255};
            float grain = std::sin((x * 0.15f) + (y * 0.045f) + plank) * 0.5f + 0.5f;
            ImageDrawPixel(&image, x, y, Blend(Jitter(wood, x, y, 14, 53), Color{166, 100, 52, 255}, grain * 0.20f));
        }
    }

    for (int x = 23; x < image.width; x += 24) {
        ImageDrawRectangle(&image, x, 0, 3, image.height, Color{50, 30, 20, 255});
    }
    ImageDrawRectangle(&image, 8, 12, 112, 8, Color{45, 45, 46, 255});
    ImageDrawRectangle(&image, 8, 86, 112, 8, Color{45, 45, 46, 255});
    ImageDrawRectangle(&image, 8, 160, 112, 8, Color{45, 45, 46, 255});
    ImageDrawRectangleLines(&image, {6, 6, 116, 180}, 4, Color{50, 30, 20, 255});
    ImageDrawCircle(&image, 96, 96, 8, GOLD);
    ImageDrawCircle(&image, 96, 96, 3, Color{80, 48, 20, 255});
    return image;
}

void SaveVisualAssetImages() {
    MakeDirectory("assets");
    MakeDirectory("assets/generated");

    Image ground = CreateGroundImage();
    ExportImage(ground, "assets/generated/ground.png");
    UnloadImage(ground);

    Image wall = CreateBrickImage();
    ExportImage(wall, "assets/generated/brick_wall.png");
    UnloadImage(wall);

    Image door = CreateDoorImage();
    ExportImage(door, "assets/generated/wooden_door.png");
    UnloadImage(door);
}

Texture2D LoadGeneratedTexture(Image image, const char* exportPath) {
    ExportImage(image, exportPath);
    Texture2D texture = LoadTextureFromImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(image);
    return texture;
}

GameVisuals LoadGameVisuals() {
    MakeDirectory("assets");
    MakeDirectory("assets/generated");

    GameVisuals visuals{};
    visuals.ground = LoadGeneratedTexture(CreateGroundImage(), "assets/generated/ground.png");
    visuals.wall = LoadGeneratedTexture(CreateBrickImage(), "assets/generated/brick_wall.png");
    visuals.door = LoadGeneratedTexture(CreateDoorImage(), "assets/generated/wooden_door.png");
    visuals.groundModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    visuals.wallModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    visuals.doorModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    SetMaterialTexture(&visuals.groundModel.materials[0], MATERIAL_MAP_DIFFUSE, visuals.ground);
    SetMaterialTexture(&visuals.wallModel.materials[0], MATERIAL_MAP_DIFFUSE, visuals.wall);
    SetMaterialTexture(&visuals.doorModel.materials[0], MATERIAL_MAP_DIFFUSE, visuals.door);
    visuals.ready = true;
    return visuals;
}

void UnloadGameVisuals(GameVisuals& visuals) {
    if (!visuals.ready) return;
    UnloadModel(visuals.groundModel);
    UnloadModel(visuals.wallModel);
    UnloadModel(visuals.doorModel);
    UnloadTexture(visuals.ground);
    UnloadTexture(visuals.wall);
    UnloadTexture(visuals.door);
    visuals.ready = false;
}

void DrawTexturedBox(Model model, Vector3 position, float width, float height, float depth, Color tint) {
    DrawModelEx(model, position, {0.0f, 1.0f, 0.0f}, 0.0f, {width, height, depth}, tint);
}

Wave MakeToneWave(float frequency, float durationSeconds, float volume, bool square = false) {
    const int sampleRate = 44100;
    const int sampleCount = static_cast<int>(sampleRate * durationSeconds);
    auto* samples = static_cast<short*>(MemAlloc(sampleCount * sizeof(short)));
    for (int i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float env = std::min(1.0f, t * 14.0f) * std::min(1.0f, (durationSeconds - t) * 18.0f);
        float s = std::sin(2.0f * PI * frequency * t);
        if (square) s = s >= 0.0f ? 1.0f : -1.0f;
        samples[i] = static_cast<short>(std::clamp(s * env * volume, -1.0f, 1.0f) * 32000.0f);
    }
    Wave wave{};
    wave.frameCount = sampleCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;
    return wave;
}

Sound MakeToneSound(float frequency, float durationSeconds, float volume, bool square = false) {
    Wave wave = MakeToneWave(frequency, durationSeconds, volume, square);
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

GameAssets LoadGameAssets() {
    GameAssets assets{};
    InitAudioDevice();
    assets.audioReady = IsAudioDeviceReady();
    if (!assets.audioReady) {
        return assets;
    }

    assets.coin = MakeToneSound(920.0f, 0.11f, 0.55f);
    assets.key = MakeToneSound(620.0f, 0.16f, 0.55f);
    assets.trap = MakeToneSound(170.0f, 0.22f, 0.75f, true);
    assets.hint = MakeToneSound(480.0f, 0.18f, 0.45f);
    assets.win = MakeToneSound(740.0f, 0.38f, 0.60f);
    assets.lose = MakeToneSound(150.0f, 0.40f, 0.75f, true);
    assets.enemy = MakeToneSound(260.0f, 0.10f, 0.35f);
    assets.step = MakeToneSound(120.0f, 0.05f, 0.18f, true);

    SetSoundVolume(assets.coin, 0.45f);
    SetSoundVolume(assets.key, 0.45f);
    SetSoundVolume(assets.trap, 0.35f);
    SetSoundVolume(assets.hint, 0.35f);
    SetSoundVolume(assets.win, 0.45f);
    SetSoundVolume(assets.lose, 0.40f);
    SetSoundVolume(assets.enemy, 0.25f);
    SetSoundVolume(assets.step, 0.18f);

    return assets;
}

void UnloadGameAssets(GameAssets& assets) {
    if (!assets.audioReady) return;
    UnloadSound(assets.coin);
    UnloadSound(assets.key);
    UnloadSound(assets.trap);
    UnloadSound(assets.hint);
    UnloadSound(assets.win);
    UnloadSound(assets.lose);
    UnloadSound(assets.enemy);
    UnloadSound(assets.step);
    CloseAudioDevice();
    assets.audioReady = false;
}

void PlayIfReady(const Sound& sound, const GameAssets& assets) {
    if (assets.audioReady) PlaySound(sound);
}

void DrawCloud(float x, float y, float scale, float alpha) {
    Color cloud = Fade(WHITE, alpha);
    Color shade = Fade(Color{210, 225, 235, 255}, alpha * 0.65f);
    DrawEllipse(static_cast<int>(x), static_cast<int>(y + 24.0f * scale), 82.0f * scale, 25.0f * scale, shade);
    DrawCircle(static_cast<int>(x - 48.0f * scale), static_cast<int>(y + 8.0f * scale), 28.0f * scale, cloud);
    DrawCircle(static_cast<int>(x - 12.0f * scale), static_cast<int>(y - 6.0f * scale), 38.0f * scale, cloud);
    DrawCircle(static_cast<int>(x + 34.0f * scale), static_cast<int>(y + 6.0f * scale), 32.0f * scale, cloud);
    DrawCircle(static_cast<int>(x + 70.0f * scale), static_cast<int>(y + 18.0f * scale), 22.0f * scale, cloud);
    DrawRectangleRounded({x - 72.0f * scale, y + 8.0f * scale, 150.0f * scale, 36.0f * scale}, 0.45f, 12, cloud);
}

void DrawSkyBackdrop(int screenWidth, int screenHeight) {
    ClearBackground(Color{80, 168, 232, 255});

    for (int y = 0; y < screenHeight; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(screenHeight);
        DrawRectangle(0, y, screenWidth, 1, Blend(Color{80, 168, 232, 255}, Color{205, 236, 252, 255}, t));
    }

    int sunX = screenWidth - 180;
    int sunY = 112;
    DrawCircleGradient(sunX, sunY, 92.0f, Fade(GOLD, 0.62f), Fade(GOLD, 0.0f));
    DrawCircle(sunX, sunY, 42.0f, Color{255, 218, 82, 255});
    DrawCircle(sunX - 10, sunY - 12, 12.0f, Fade(WHITE, 0.35f));

    DrawCloud(185.0f, 118.0f, 0.95f, 0.90f);
    DrawCloud(465.0f, 82.0f, 0.72f, 0.72f);
    DrawCloud(screenWidth - 420.0f, 185.0f, 1.08f, 0.78f);
    DrawRectangle(0, static_cast<int>(screenHeight * 0.72f), screenWidth, static_cast<int>(screenHeight * 0.28f), Fade(Color{119, 174, 94, 255}, 0.15f));
}

struct GameContext {
    Maze maze{12, 12};
    Difficulty difficulty = Difficulty::Medium;
    DifficultySettings settings{};
    PlayerState player{};
    EnemyState enemy{};
    GameScreenState state = GameScreenState::Menu;
    std::vector<std::pair<int, int>> hintPath;
    bool showHint = false;
    bool showMinimap = true;
    float timeLeft = 0.0f;
    float totalTime = 0.0f;
    int score = 0;
    float coinSpin = 0.0f;
    std::string message;
    float messageTimer = 0.0f;
};

DifficultySettings GetSettings(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return {{10, 10, 1, 10, 0, false}, 240.0f, 6, 100, false, false, "Easy"};
        case Difficulty::Medium:
            return {{14, 14, 1, 14, 4, false}, 210.0f, 4, 250, true, false, "Medium"};
        case Difficulty::Hard:
        default:
            return {{18, 18, 1, 18, 8, true}, 190.0f, 3, 500, true, true, "Hard"};
    }
}

Vector3 CellCenter(int row, int col, float y = 0.65f) {
    return {
        col * kCellSize + kCellSize * 0.5f,
        y,
        row * kCellSize + kCellSize * 0.5f
    };
}

float Pulse(float speed = 3.0f, float phase = 0.0f) {
    return 0.5f + 0.5f * std::sin(GetTime() * speed + phase);
}

Vector2 CellCenter2D(int row, int col) {
    return {
        col * kCellSize + kCellSize * 0.5f,
        row * kCellSize + kCellSize * 0.5f
    };
}

std::pair<int, int> CellFromWorldPosition(const Maze& maze, Vector2 position) {
    int col = static_cast<int>(std::floor(position.x / kCellSize));
    int row = static_cast<int>(std::floor(position.y / kCellSize));
    row = std::clamp(row, 0, maze.GetRows() - 1);
    col = std::clamp(col, 0, maze.GetCols() - 1);
    return {row, col};
}

float PlayerJumpOffset(const PlayerState& player) {
    if (!player.jumping) {
        return 0.0f;
    }
    float t = std::clamp(player.jumpTimer / kJumpDuration, 0.0f, 1.0f);
    return std::sin(t * PI) * kJumpHeight;
}

Vector3 PlayerWorldPosition(const PlayerState& player) {
    return {player.position.x, 0.65f + PlayerJumpOffset(player), player.position.y};
}

bool CanStandAtPosition(const Maze& maze, Vector2 position, bool hasKey) {
    auto [row, col] = CellFromWorldPosition(maze, position);
    if (!maze.InBounds(row, col)) {
        return false;
    }

    float localX = position.x - col * kCellSize;
    float localZ = position.y - row * kCellSize;
    if (localX < 0.0f || localX >= kCellSize || localZ < 0.0f || localZ >= kCellSize) {
        return false;
    }

    if (localZ < kPlayerRadius && !maze.CanMove(row, col, 0, hasKey)) return false;
    if (localX > kCellSize - kPlayerRadius && !maze.CanMove(row, col, 1, hasKey)) return false;
    if (localZ > kCellSize - kPlayerRadius && !maze.CanMove(row, col, 2, hasKey)) return false;
    if (localX < kPlayerRadius && !maze.CanMove(row, col, 3, hasKey)) return false;
    return true;
}

bool IsMenuSelectionPressed(int index) {
    return (index == 0 && IsKeyPressed(KEY_ONE)) || (index == 1 && IsKeyPressed(KEY_TWO)) || (index == 2 && IsKeyPressed(KEY_THREE));
}

Rectangle MenuButtonRect(int screenWidth, int screenHeight, int index) {
    float width = std::min(720.0f, static_cast<float>(screenWidth - 96));
    float height = 74.0f;
    float gap = 18.0f;
    float startY = std::max(236.0f, screenHeight * 0.34f);
    return {
        (screenWidth - width) * 0.5f,
        startY + index * (height + gap),
        width,
        height
    };
}

Vector2 MovementVectorFromInput(const PlayerState& player) {
    Vector2 input = {0.0f, 0.0f};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) input.y += 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) input.y -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.x += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.x -= 1.0f;

    if (input.x == 0.0f && input.y == 0.0f) {
        return {0.0f, 0.0f};
    }

    float sinYaw = std::sin(player.yaw);
    float cosYaw = std::cos(player.yaw);
    Vector2 forward = {sinYaw, cosYaw};
    Vector2 right = {-cosYaw, sinYaw};
    Vector2 desired = Vector2Add(Vector2Scale(forward, input.y), Vector2Scale(right, input.x));
    return Vector2Normalize(desired);
}

void ShowMessage(GameContext& game, const std::string& text, float duration = 2.0f) {
    game.message = text;
    game.messageTimer = duration;
}

void StartNewRun(GameContext& game, Difficulty difficulty, const GameAssets& assets) {
    game.difficulty = difficulty;
    game.settings = GetSettings(difficulty);
    game.maze.Generate(game.settings.config);
    game.player = {};
    game.player.cell = game.maze.GetStart();
    game.player.fromCell = game.player.cell;
    game.player.toCell = game.player.cell;
    game.player.position = CellCenter2D(game.player.cell.first, game.player.cell.second);
    game.player.yaw = 0.0f;
    game.player.pitch = -0.25f;
    game.enemy = {};
    game.enemy.active = game.settings.enemyEnabled;
    game.enemy.cell = game.maze.GetGoal();
    game.enemy.moveTimer = kEnemyMoveInterval;
    game.hintPath.clear();
    game.showHint = false;
    game.timeLeft = game.settings.timeLimit;
    game.totalTime = 0.0f;
    game.score = 0;
    game.message.clear();
    game.messageTimer = 0.0f;
    game.state = GameScreenState::Playing;
    DisableCursor();
}

void HandleFeaturePickup(GameContext& game, const GameAssets& assets) {
    Cell& cell = game.maze.GetMutableCell(game.player.cell.first, game.player.cell.second);
    if (cell.collected) {
        return;
    }

    switch (cell.feature) {
        case CellFeature::Key:
            game.player.keys += 1;
            game.score += 150;
            ShowMessage(game, "Key collected. The locked door is now passable.");
            PlayIfReady(assets.key, assets);
            cell.collected = true;
            break;
        case CellFeature::Coin:
            game.player.coins += 1;
            game.score += 25;
            ShowMessage(game, "Coin collected.", 1.2f);
            PlayIfReady(assets.coin, assets);
            cell.collected = true;
            break;
        case CellFeature::Trap:
            if (game.settings.useTraps) {
                if (game.player.jumping) {
                    ShowMessage(game, "Trap jumped.", 1.0f);
                    cell.collected = true;
                    break;
                }
                game.timeLeft = std::max(0.0f, game.timeLeft - 8.0f);
                ShowMessage(game, "Trap triggered. Lost 8 seconds.");
                PlayIfReady(assets.trap, assets);
            }
            cell.collected = true;
            break;
        default:
            break;
    }
}

void UpdateEnemy(GameContext& game, float dt, const GameAssets& assets) {
    if (!game.enemy.active || game.state != GameScreenState::Playing) {
        return;
    }

    game.enemy.moveTimer -= dt;
    if (game.enemy.moveTimer > 0.0f) {
        return;
    }
    game.enemy.moveTimer = kEnemyMoveInterval;

    bool enemyHasKey = true;
    auto path = Solver::SolveAStar(game.maze, game.enemy.cell, game.player.cell, enemyHasKey);
    if (path.size() > 1) {
        game.enemy.cell = path[1];
        PlayIfReady(assets.enemy, assets);
    }

    if (game.enemy.cell == game.player.cell) {
        bool playerIsSprinting = game.player.moving && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        if (playerIsSprinting) {
            game.enemy.moveTimer = kEnemyMoveInterval * 1.6f;
            ShowMessage(game, "Sprinted past the hunter.", 0.8f);
            return;
        }
        game.state = GameScreenState::Lost;
        EnableCursor();
        ShowMessage(game, "The hunter caught you.");
        PlayIfReady(assets.lose, assets);
    }
}

void UpdatePlaying(GameContext& game, Camera3D& camera, const GameAssets& assets) {
    float dt = GetFrameTime();
    game.coinSpin += dt * kCoinSpinSpeed;

    if (game.messageTimer > 0.0f) {
        game.messageTimer -= dt;
        if (game.messageTimer <= 0.0f) {
            game.message.clear();
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        game.state = GameScreenState::Paused;
        EnableCursor();
        return;
    }

    if (IsKeyPressed(KEY_M)) {
        game.showMinimap = !game.showMinimap;
    }

    if (IsKeyPressed(KEY_R)) {
        StartNewRun(game, game.difficulty, assets);
        return;
    }

    if (IsKeyPressed(KEY_H) && game.settings.hintCharges > 0) {
        game.hintPath = Solver::SolveBFS(game.maze, game.player.cell, game.maze.GetGoal(), game.player.keys > 0);
        game.showHint = true;
        game.settings.hintCharges -= 1;
        game.player.hintsUsed += 1;
        game.score = std::max(0, game.score - 20);
        ShowMessage(game, "Hint path refreshed.", 1.5f);
        PlayIfReady(assets.hint, assets);
    }

    Vector2 mouseDelta = GetMouseDelta();
    game.player.yaw -= mouseDelta.x * kMouseSensitivity;
    game.player.pitch -= mouseDelta.y * kMouseSensitivity;
    game.player.pitch = std::clamp(game.player.pitch, -0.85f, 0.65f);

    game.timeLeft -= dt;
    game.totalTime += dt;
    if (game.timeLeft <= 0.0f) {
        game.timeLeft = 0.0f;
        game.state = GameScreenState::Lost;
        EnableCursor();
        ShowMessage(game, "Time is up.");
        PlayIfReady(assets.lose, assets);
        return;
    }

    if (IsKeyPressed(KEY_SPACE) && !game.player.jumping) {
        game.player.jumping = true;
        game.player.jumpTimer = 0.0f;
    }

    if (game.player.jumping) {
        game.player.jumpTimer += dt;
        if (game.player.jumpTimer >= kJumpDuration) {
            game.player.jumping = false;
            game.player.jumpTimer = 0.0f;
            HandleFeaturePickup(game, assets);
        }
    }

    Vector2 moveDir = MovementVectorFromInput(game.player);
    game.player.moving = Vector2LengthSqr(moveDir) > 0.0f;
    if (game.player.moving) {
        float speed = kWalkSpeed * (IsKeyDown(KEY_LEFT_SHIFT) ? kSprintMultiplier : 1.0f);
        Vector2 delta = Vector2Scale(moveDir, speed * dt);
        Vector2 nextX = {game.player.position.x + delta.x, game.player.position.y};
        if (CanStandAtPosition(game.maze, nextX, game.player.keys > 0)) {
            game.player.position = nextX;
        }
        Vector2 nextZ = {game.player.position.x, game.player.position.y + delta.y};
        if (CanStandAtPosition(game.maze, nextZ, game.player.keys > 0)) {
            game.player.position = nextZ;
        }

        auto newCell = CellFromWorldPosition(game.maze, game.player.position);
        if (newCell != game.player.cell) {
            game.player.fromCell = game.player.cell;
            game.player.cell = newCell;
            game.player.toCell = newCell;
            game.showHint = false;
            HandleFeaturePickup(game, assets);
            PlayIfReady(assets.step, assets);
        }
    }

    if (game.player.cell == game.maze.GetGoal()) {
        int timeBonus = static_cast<int>(game.timeLeft * 4.0f);
        int hintPenalty = game.player.hintsUsed * 25;
        int coinBonus = game.player.coins * 25;
        game.score += game.settings.scoreBonus + timeBonus + coinBonus - hintPenalty;
        game.state = GameScreenState::Won;
        EnableCursor();
        ShowMessage(game, "You escaped the maze.");
        PlayIfReady(assets.win, assets);
    }

    UpdateEnemy(game, dt, assets);

    Vector3 playerPos = PlayerWorldPosition(game.player);
    Vector3 eyeOffset = {0.0f, 1.35f, 0.0f};
    Vector3 eyePos = Vector3Add(playerPos, eyeOffset);
    Vector3 forward = {
        std::sin(game.player.yaw) * std::cos(game.player.pitch),
        std::sin(game.player.pitch),
        std::cos(game.player.yaw) * std::cos(game.player.pitch)
    };

    camera.position = eyePos;
    camera.target = Vector3Add(eyePos, forward);
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void DrawFinishLine(Vector3 center) {
    const float tileW = 0.56f;
    const float tileD = 0.34f;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            Color color = ((row + col) % 2 == 0) ? RAYWHITE : BLACK;
            DrawCube({center.x - 1.12f + col * tileW, 0.055f, center.z - 0.72f + row * tileD}, tileW, 0.06f, tileD, color);
        }
    }

    Color glow = Color{85, 235, 105, 255};
    DrawCylinderEx({center.x - 1.45f, 0.05f, center.z + 1.05f}, {center.x - 1.45f, 2.7f, center.z + 1.05f}, 0.10f, 0.16f, 16, glow);
    DrawCylinderEx({center.x + 1.45f, 0.05f, center.z + 1.05f}, {center.x + 1.45f, 2.7f, center.z + 1.05f}, 0.10f, 0.16f, 16, glow);
    DrawCylinderEx({center.x - 1.45f, 2.7f, center.z + 1.05f}, {center.x + 1.45f, 2.7f, center.z + 1.05f}, 0.14f, 0.14f, 16, LIME);
    DrawCubeWires({center.x, 1.42f, center.z + 1.05f}, 3.15f, 2.95f, 0.16f, Fade(LIME, 0.85f));
    DrawSphere({center.x, 1.55f + 0.18f * Pulse(5.0f), center.z + 1.0f}, 0.55f, Fade(Color{70, 255, 125, 255}, 0.38f));
}

void DrawFeature(const Maze& maze, int r, int c, float spinAngle, const GameVisuals& visuals) {
    const Cell& cell = maze.GetCell(r, c);
    if (cell.collected) {
        return;
    }

    Vector3 center = CellCenter(r, c, 0.55f);
    float bob = 0.08f * std::sin(GetTime() * 3.0f + r * 0.6f + c * 0.4f);
    switch (cell.feature) {
        case CellFeature::Exit:
            DrawFinishLine(center);
            break;
        case CellFeature::Key:
            DrawSphere({center.x, 0.95f + bob, center.z}, 0.62f, Fade(GOLD, 0.16f));
            DrawCylinderEx({center.x - 0.45f, 1.05f + bob, center.z - 0.05f}, {center.x - 0.45f, 1.05f + bob, center.z + 0.05f}, 0.31f, 0.31f, 28, GOLD);
            DrawCylinderEx({center.x - 0.45f, 1.05f + bob, center.z - 0.065f}, {center.x - 0.45f, 1.05f + bob, center.z + 0.065f}, 0.18f, 0.18f, 28, Color{45, 36, 22, 255});
            DrawCylinderWiresEx({center.x - 0.45f, 1.05f + bob, center.z - 0.07f}, {center.x - 0.45f, 1.05f + bob, center.z + 0.07f}, 0.31f, 0.31f, 28, YELLOW);
            DrawCylinderEx({center.x - 0.20f, 1.05f + bob, center.z}, {center.x + 0.70f, 1.05f + bob, center.z}, 0.075f, 0.075f, 16, GOLD);
            DrawCube({center.x + 0.52f, 0.88f + bob, center.z}, 0.12f, 0.34f, 0.12f, GOLD);
            DrawCube({center.x + 0.72f, 0.92f + bob, center.z}, 0.12f, 0.26f, 0.12f, Color{245, 190, 45, 255});
            DrawSphere({center.x + 0.18f, 1.20f + bob, center.z - 0.08f}, 0.06f, Fade(WHITE, 0.65f));
            break;
        case CellFeature::Door:
            {
                int eastWestOpen = (!cell.walls[1] ? 1 : 0) + (!cell.walls[3] ? 1 : 0);
                int northSouthOpen = (!cell.walls[0] ? 1 : 0) + (!cell.walls[2] ? 1 : 0);
                bool eastWestDoor = eastWestOpen > northSouthOpen;
                Vector3 doorSize = eastWestDoor ? Vector3{0.32f, 2.9f, 2.25f} : Vector3{2.25f, 2.9f, 0.32f};
                Vector3 frameSize = eastWestDoor ? Vector3{0.46f, 3.15f, 0.20f} : Vector3{0.20f, 3.15f, 0.46f};
                Vector3 headerSize = eastWestDoor ? Vector3{0.52f, 0.28f, 2.70f} : Vector3{2.70f, 0.28f, 0.52f};

                if (visuals.ready) {
                    DrawTexturedBox(visuals.doorModel, {center.x, 1.45f, center.z}, doorSize.x, doorSize.y, doorSize.z, WHITE);
                } else {
                    DrawCube({center.x, 1.45f, center.z}, doorSize.x, doorSize.y, doorSize.z, Color{120, 72, 40, 255});
                }

                if (eastWestDoor) {
                    DrawCube({center.x, 1.55f, center.z - 1.25f}, frameSize.x, frameSize.y, frameSize.z, Color{72, 55, 42, 255});
                    DrawCube({center.x, 1.55f, center.z + 1.25f}, frameSize.x, frameSize.y, frameSize.z, Color{72, 55, 42, 255});
                    DrawSphere({center.x + 0.21f, 1.33f, center.z + 0.82f}, 0.11f, GOLD);
                    DrawCylinderEx({center.x + 0.22f, 1.12f, center.z + 0.78f}, {center.x + 0.22f, 0.92f, center.z + 0.78f}, 0.035f, 0.055f, 10, Color{38, 26, 18, 255});
                } else {
                    DrawCube({center.x - 1.25f, 1.55f, center.z}, frameSize.x, frameSize.y, frameSize.z, Color{72, 55, 42, 255});
                    DrawCube({center.x + 1.25f, 1.55f, center.z}, frameSize.x, frameSize.y, frameSize.z, Color{72, 55, 42, 255});
                    DrawSphere({center.x + 0.82f, 1.33f, center.z + 0.21f}, 0.11f, GOLD);
                    DrawCylinderEx({center.x + 0.78f, 1.12f, center.z + 0.22f}, {center.x + 0.78f, 0.92f, center.z + 0.22f}, 0.035f, 0.055f, 10, Color{38, 26, 18, 255});
                }

                DrawCube({center.x, 3.05f, center.z}, headerSize.x, headerSize.y, headerSize.z, Color{72, 55, 42, 255});
                DrawCubeWires({center.x, 1.45f, center.z}, doorSize.x + 0.07f, doorSize.y + 0.06f, doorSize.z + 0.04f, Fade(GOLD, 0.85f));
            }
            break;
        case CellFeature::Coin: {
            float angle = spinAngle * DEG2RAD;
            Vector3 axis{std::cos(angle), 0.0f, std::sin(angle)};
            Vector3 p1{center.x - axis.x * 0.055f, 1.02f + bob, center.z - axis.z * 0.055f};
            Vector3 p2{center.x + axis.x * 0.055f, 1.02f + bob, center.z + axis.z * 0.055f};
            DrawSphere({center.x, 1.02f + bob, center.z}, 0.56f, Color{255, 203, 28, 255});
            DrawCylinderEx(p1, p2, 0.50f, 0.50f, 48, Color{245, 144, 10, 255});
            DrawCylinderEx(p1, p2, 0.38f, 0.38f, 44, Color{255, 226, 45, 255});
            DrawCylinderEx(p1, p2, 0.17f, 0.17f, 32, Color{255, 250, 138, 255});
            DrawCylinderWiresEx(p1, p2, 0.49f, 0.49f, 48, Color{255, 112, 0, 255});
            DrawSphere({center.x + axis.x * 0.08f, 1.18f + bob, center.z + axis.z * 0.08f}, 0.07f, Fade(WHITE, 0.8f));
            break;
        }
        case CellFeature::Trap:
            DrawCube({center.x, 0.12f, center.z}, 1.4f, 0.1f, 1.4f, RED);
            DrawCubeWires({center.x, 0.12f, center.z}, 1.4f, 0.1f, 1.4f, MAROON);
            for (int i = -1; i <= 1; ++i) {
                DrawCube({center.x + i * 0.36f, 0.28f, center.z}, 0.08f, 0.32f + 0.08f * Pulse(7.0f, i), 0.08f, ORANGE);
            }
            break;
        case CellFeature::None:
        default:
            break;
    }
}

void DrawMaze3D(const GameContext& game, const GameVisuals& visuals) {
    const Maze& maze = game.maze;
    const float width = maze.GetCols() * kCellSize;
    const float depth = maze.GetRows() * kCellSize;
    Vector3 playerPos = PlayerWorldPosition(game.player);
    DrawPlane({width / 2.0f, -0.04f, depth / 2.0f}, {width + 26.0f, depth + 26.0f}, Color{72, 128, 63, 255});

    for (int r = 0; r < maze.GetRows(); ++r) {
        for (int c = 0; c < maze.GetCols(); ++c) {
            const Cell& cell = maze.GetCell(r, c);
            float x = c * kCellSize;
            float z = r * kCellSize;
            Vector3 tileCenter{x + kCellSize / 2.0f, 0.0f, z + kCellSize / 2.0f};
            float dist = Vector3Distance(playerPos, tileCenter);
            float nearGlow = std::clamp(1.0f - dist / 18.0f, 0.0f, 1.0f);
            Color wallTint = ColorBrightness(WHITE, -0.12f + 0.20f * nearGlow);

            if (visuals.ready) {
                DrawTexturedBox(visuals.groundModel, {x + kCellSize / 2.0f, -0.055f, z + kCellSize / 2.0f}, kCellSize, 0.08f, kCellSize, WHITE);
            } else {
                DrawCube({x + kCellSize / 2.0f, -0.055f, z + kCellSize / 2.0f}, kCellSize, 0.08f, kCellSize, Color{96, 116, 72, 255});
            }
            DrawCube({x + kCellSize / 2.0f, 0.005f, z + kCellSize / 2.0f}, kCellSize * 0.96f, 0.012f, kCellSize * 0.96f, Fade(Color{80, 140, 66, 255}, 0.10f + nearGlow * 0.06f));

            if (cell.walls[0]) {
                Vector3 pos{x + kCellSize / 2.0f, kWallHeight / 2.0f, z};
                if (visuals.ready) DrawTexturedBox(visuals.wallModel, pos, kCellSize, kWallHeight, kWallThickness, wallTint);
                else DrawCube(pos, kCellSize, kWallHeight, kWallThickness, Color{126, 65, 50, 255});
                DrawCube({pos.x, kWallHeight + 0.06f, pos.z}, kCellSize, 0.12f, kWallThickness + 0.08f, Color{96, 76, 60, 255});
                DrawCubeWires(pos, kCellSize, kWallHeight, kWallThickness, Fade(Color{40, 28, 22, 255}, 0.5f));
            }
            if (cell.walls[1]) {
                Vector3 pos{x + kCellSize, kWallHeight / 2.0f, z + kCellSize / 2.0f};
                if (visuals.ready) DrawTexturedBox(visuals.wallModel, pos, kWallThickness, kWallHeight, kCellSize, wallTint);
                else DrawCube(pos, kWallThickness, kWallHeight, kCellSize, Color{126, 65, 50, 255});
                DrawCube({pos.x, kWallHeight + 0.06f, pos.z}, kWallThickness + 0.08f, 0.12f, kCellSize, Color{96, 76, 60, 255});
                DrawCubeWires(pos, kWallThickness, kWallHeight, kCellSize, Fade(Color{40, 28, 22, 255}, 0.5f));
            }
            if (r == maze.GetRows() - 1 && cell.walls[2]) {
                Vector3 pos{x + kCellSize / 2.0f, kWallHeight / 2.0f, z + kCellSize};
                if (visuals.ready) DrawTexturedBox(visuals.wallModel, pos, kCellSize, kWallHeight, kWallThickness, wallTint);
                else DrawCube(pos, kCellSize, kWallHeight, kWallThickness, Color{126, 65, 50, 255});
                DrawCube({pos.x, kWallHeight + 0.06f, pos.z}, kCellSize, 0.12f, kWallThickness + 0.08f, Color{96, 76, 60, 255});
                DrawCubeWires(pos, kCellSize, kWallHeight, kWallThickness, Fade(Color{40, 28, 22, 255}, 0.5f));
            }
            if (c == 0 && cell.walls[3]) {
                Vector3 pos{x, kWallHeight / 2.0f, z + kCellSize / 2.0f};
                if (visuals.ready) DrawTexturedBox(visuals.wallModel, pos, kWallThickness, kWallHeight, kCellSize, wallTint);
                else DrawCube(pos, kWallThickness, kWallHeight, kCellSize, Color{126, 65, 50, 255});
                DrawCube({pos.x, kWallHeight + 0.06f, pos.z}, kWallThickness + 0.08f, 0.12f, kCellSize, Color{96, 76, 60, 255});
                DrawCubeWires(pos, kWallThickness, kWallHeight, kCellSize, Fade(Color{40, 28, 22, 255}, 0.5f));
            }

            DrawFeature(maze, r, c, game.coinSpin, visuals);
        }
    }
}

void DrawHintPath(const std::vector<std::pair<int, int>>& path) {
    for (const auto& [r, c] : path) {
        Vector3 p = CellCenter(r, c, 0.08f);
        DrawSphere(p, 0.12f, SKYBLUE);
    }
}

void DrawMinimap(const GameContext& game, int screenWidth, int screenHeight) {
    if (!game.showMinimap) {
        return;
    }

    const Maze& maze = game.maze;
    const int miniSize = 180;
    const int x0 = screenWidth - miniSize - 24;
    const int y0 = 24;
    const float cw = static_cast<float>(miniSize) / maze.GetCols();
    const float ch = static_cast<float>(miniSize) / maze.GetRows();

    DrawRectangleRounded({static_cast<float>(x0 - 10), static_cast<float>(y0 - 10), static_cast<float>(miniSize + 20), static_cast<float>(miniSize + 20)}, 0.18f, 8, Fade(BLACK, 0.5f));
    DrawRectangle(x0, y0, miniSize, miniSize, Fade(Color{15, 18, 24, 255}, 0.95f));

    for (int r = 0; r < maze.GetRows(); ++r) {
        for (int c = 0; c < maze.GetCols(); ++c) {
            const Cell& cell = maze.GetCell(r, c);
            Rectangle rect{ x0 + c * cw, y0 + r * ch, cw, ch };
            Color fill = Color{35, 40, 52, 255};
            if (cell.feature == CellFeature::Exit) fill = DARKGREEN;
            if (cell.feature == CellFeature::Door) fill = BROWN;
            if (cell.feature == CellFeature::Trap && !cell.collected) fill = MAROON;
            DrawRectangleRec(rect, fill);

            if (cell.walls[0]) DrawLineEx({rect.x, rect.y}, {rect.x + rect.width, rect.y}, 2.0f, RAYWHITE);
            if (cell.walls[1]) DrawLineEx({rect.x + rect.width, rect.y}, {rect.x + rect.width, rect.y + rect.height}, 2.0f, RAYWHITE);
            if (r == maze.GetRows() - 1 && cell.walls[2]) DrawLineEx({rect.x, rect.y + rect.height}, {rect.x + rect.width, rect.y + rect.height}, 2.0f, RAYWHITE);
            if (c == 0 && cell.walls[3]) DrawLineEx({rect.x, rect.y}, {rect.x, rect.y + rect.height}, 2.0f, RAYWHITE);
        }
    }

    float px = x0 + (game.player.cell.second + 0.5f) * cw;
    float py = y0 + (game.player.cell.first + 0.5f) * ch;
    DrawCircle(static_cast<int>(px), static_cast<int>(py), 5.0f, YELLOW);

    if (game.enemy.active) {
        float ex = x0 + (game.enemy.cell.second + 0.5f) * cw;
        float ey = y0 + (game.enemy.cell.first + 0.5f) * ch;
        DrawCircle(static_cast<int>(ex), static_cast<int>(ey), 5.0f, RED);
    }

    DrawText("Minimap (M)", x0, y0 + miniSize + 8, 18, RAYWHITE);
}

void DrawHud(const GameContext& game, int screenWidth, int screenHeight) {
    DrawRectangleRounded({18.0f, 18.0f, static_cast<float>(kHudWidth), 185.0f}, 0.2f, 10, Fade(BLACK, 0.42f));
    DrawText("MazeScape 3D Deluxe", 34, 30, 28, RAYWHITE);
    DrawText(TextFormat("Mode: %s", game.settings.label), 34, 66, 20, SKYBLUE);
    DrawText(TextFormat("Time: %.1f", game.timeLeft), 34, 92, 22, YELLOW);
    DrawText(TextFormat("Score: %d", game.score), 200, 92, 22, LIME);
    DrawText(TextFormat("Keys: %d", game.player.keys), 34, 120, 20, GOLD);
    DrawText(TextFormat("Coins: %d", game.player.coins), 140, 120, 20, ORANGE);
    DrawText(TextFormat("Hints left: %d", game.settings.hintCharges), 250, 120, 20, SKYBLUE);
    DrawText("WASD steps | Space jump | Shift sprint | H hint | M minimap", 34, 148, 18, LIGHTGRAY);
    DrawText("R restart | Esc pause", 34, 170, 18, LIGHTGRAY);

    if (!game.message.empty()) {
        int w = MeasureText(game.message.c_str(), 24) + 40;
        DrawRectangleRounded({(screenWidth - w) / 2.0f, static_cast<float>(screenHeight - 92), static_cast<float>(w), 48.0f}, 0.2f, 8, Fade(DARKBLUE, 0.7f));
        DrawText(game.message.c_str(), (screenWidth - w) / 2 + 20, screenHeight - 79, 24, RAYWHITE);
    }
}

void DrawOverlayCard(int screenWidth, int screenHeight, const char* title, const char* subtitle, const char* footer, Color accent) {
    float w = 620.0f;
    float h = 280.0f;
    float x = (screenWidth - w) / 2.0f;
    float y = (screenHeight - h) / 2.0f;
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.55f));
    DrawRectangleRounded({x, y, w, h}, 0.12f, 10, Fade(Color{24, 28, 37, 255}, 0.96f));
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.12f, 10, 3.0f, accent);
    DrawText(title, static_cast<int>(x + 32), static_cast<int>(y + 36), 40, RAYWHITE);
    DrawText(subtitle, static_cast<int>(x + 32), static_cast<int>(y + 98), 24, LIGHTGRAY);
    DrawText(footer, static_cast<int>(x + 32), static_cast<int>(y + 210), 22, accent);
}

void DrawMenu(const GameContext& game, int screenWidth, int screenHeight) {
    float t = Pulse(2.0f);
    DrawSkyBackdrop(screenWidth, screenHeight);
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(Color{20, 30, 24, 255}, 0.12f));
    DrawCircleGradient(screenWidth / 2, screenHeight / 3, 240.0f + 60.0f * t, Fade(WHITE, 0.28f), Fade(WHITE, 0.0f));

    const char* title = "MazeScape 3D Deluxe";
    const char* subtitle = "Pick a mode and escape before time runs out";
    DrawText(title, screenWidth / 2 - MeasureText(title, 52) / 2, 82, 52, Color{22, 38, 46, 255});
    DrawText(subtitle, screenWidth / 2 - MeasureText(subtitle, 24) / 2, 150, 24, Color{32, 82, 106, 255});

    const char* labels[3] = {
        "1  Easy",
        "2  Medium",
        "3  Hard"
    };
    const char* details[3] = {
        "Long timer, more hints, no traps",
        "Traps enabled, balanced maze",
        "Larger maze, traps, hunter AI"
    };
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 3; ++i) {
        Rectangle box = MenuButtonRect(screenWidth, screenHeight, i);
        bool hover = CheckCollisionPointRec(mouse, box);
        Color fill = hover ? Color{255, 252, 224, 245} : Color{238, 248, 250, 238};
        Color outline = hover ? GOLD : Fade(Color{52, 92, 108, 255}, 0.85f);
        DrawRectangleRounded(box, 0.12f, 10, fill);
        DrawRectangleRoundedLinesEx(box, 0.12f, 10, hover ? 3.0f : 2.0f, outline);
        DrawText(labels[i], static_cast<int>(box.x + 24), static_cast<int>(box.y + 12), 28, Color{22, 38, 46, 255});
        DrawText(details[i], static_cast<int>(box.x + 24), static_cast<int>(box.y + 44), 18, Color{48, 76, 82, 255});
    }

    const char* startHint = "Click a mode or press 1 / 2 / 3";
    const char* controls = "WASD move in steps | Space jumps traps in Medium and Hard | Shift sprint";
    DrawText(startHint, screenWidth / 2 - MeasureText(startHint, 26) / 2, screenHeight - 132, 26, Color{118, 86, 16, 255});
    DrawText(controls, screenWidth / 2 - MeasureText(controls, 20) / 2, screenHeight - 92, 20, Color{34, 58, 64, 255});
}
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--export-assets") {
        SaveVisualAssetImages();
        return 0;
    }

    const int screenWidth = 1440;
    const int screenHeight = 900;
    InitWindow(screenWidth, screenHeight, "MazeScape 3D Deluxe");
    SetTargetFPS(60);

    GameContext game;
    game.settings = GetSettings(game.difficulty);
    GameAssets assets = LoadGameAssets();
    GameVisuals visuals = LoadGameVisuals();

    Camera3D camera{};

    while (!WindowShouldClose()) {
        if (game.state == GameScreenState::Menu) {
            if (IsMenuSelectionPressed(0)) StartNewRun(game, Difficulty::Easy, assets);
            if (IsMenuSelectionPressed(1)) StartNewRun(game, Difficulty::Medium, assets);
            if (IsMenuSelectionPressed(2)) StartNewRun(game, Difficulty::Hard, assets);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                for (int i = 0; i < 3; ++i) {
                    Rectangle box = MenuButtonRect(screenWidth, screenHeight, i);
                    if (CheckCollisionPointRec(mouse, box)) {
                        StartNewRun(game, i == 0 ? Difficulty::Easy : (i == 1 ? Difficulty::Medium : Difficulty::Hard), assets);
                    }
                }
            }
        } else if (game.state == GameScreenState::Playing) {
            UpdatePlaying(game, camera, assets);
        } else if (game.state == GameScreenState::Paused) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                game.state = GameScreenState::Playing;
                DisableCursor();
            }
            if (IsKeyPressed(KEY_R)) {
                StartNewRun(game, game.difficulty, assets);
            }
            if (IsKeyPressed(KEY_BACKSPACE)) {
                game.state = GameScreenState::Menu;
            }
        } else if (game.state == GameScreenState::Won || game.state == GameScreenState::Lost) {
            if (IsKeyPressed(KEY_R)) StartNewRun(game, game.difficulty, assets);
            if (IsKeyPressed(KEY_BACKSPACE)) game.state = GameScreenState::Menu;
        }

        BeginDrawing();
        if (game.state == GameScreenState::Menu) {
            DrawMenu(game, screenWidth, screenHeight);
        } else {
            DrawSkyBackdrop(screenWidth, screenHeight);
            BeginMode3D(camera);
            DrawMaze3D(game, visuals);
            if (game.showHint && !game.hintPath.empty()) {
                DrawHintPath(game.hintPath);
            }
            DrawSphere(PlayerWorldPosition(game.player), 0.18f, YELLOW);
            if (game.enemy.active) {
                DrawSphere(CellCenter(game.enemy.cell.first, game.enemy.cell.second), 0.45f, RED);
            }
            EndMode3D();

            DrawHud(game, screenWidth, screenHeight);
            DrawMinimap(game, screenWidth, screenHeight);

            if (game.state == GameScreenState::Paused) {
                DrawOverlayCard(screenWidth, screenHeight, "Paused", "Press Esc to continue, R to restart, Backspace for menu", "Take a breath and plan your route.", SKYBLUE);
            } else if (game.state == GameScreenState::Won) {
                DrawOverlayCard(screenWidth, screenHeight, "You Escaped", TextFormat("Final score: %d | Coins: %d | Hints used: %d", game.score, game.player.coins, game.player.hintsUsed), "R to replay, Backspace for menu", LIME);
            } else if (game.state == GameScreenState::Lost) {
                DrawOverlayCard(screenWidth, screenHeight, "Run Failed", TextFormat("Score: %d | Time survived: %.1f", game.score, game.totalTime), "R to retry, Backspace for menu", RED);
            }
        }
        EndDrawing();
    }

    UnloadGameVisuals(visuals);
    UnloadGameAssets(assets);
    EnableCursor();
    CloseWindow();
    return 0;
}
