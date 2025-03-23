#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h" // Added for Matrix functions
#include "ode/ode.h"

// Physics objects
dWorldID world;
dSpaceID space;
dGeomID ground;
dBodyID cube_body;
dGeomID cube_geom;
dJointGroupID contact_group;

// Callback for collision detection
static void nearCallback(void *data, dGeomID o1, dGeomID o2) {
    dBodyID b1 = dGeomGetBody(o1);
    dBodyID b2 = dGeomGetBody(o2);
    if (b1 && b2 && dAreConnected(b1, b2)) return;

    dContact contact;
    contact.surface.mode = dContactBounce;
    contact.surface.mu = dInfinity; // Friction
    contact.surface.bounce = 0.5f;   // Bounciness
    contact.surface.bounce_vel = 0.1f;
    contact.surface.soft_cfm = 0.01f;

    if (dCollide(o1, o2, 1, &contact.geom, sizeof(dContact))) {
        dJointID c = dJointCreateContact(world, contact_group, &contact);
        dJointAttach(c, b1, b2);
    }
}

// Function to reset cube position randomly
void resetCubePosition(dBodyID body) {
    float x = (float)GetRandomValue(-5, 5);
    float z = (float)GetRandomValue(-5, 5);
    float y = (float)GetRandomValue(5, 15);
    dBodySetPosition(body, x, y, z);
    dBodySetLinearVel(body, 0, 0, 0);
    dBodySetAngularVel(body, 0, 0, 0);
}

// Convert ODE rotation matrix to yaw, pitch, roll (in degrees)
void getYawPitchRoll(const dReal *rot, float *yaw, float *pitch, float *roll) {
    float r11 = rot[0], r12 = rot[4], r13 = rot[8];
    float r21 = rot[1], r23 = rot[9];
    float r31 = rot[2], r32 = rot[6], r33 = rot[10];

    *pitch = atan2f(-r31, sqrtf(r11 * r11 + r21 * r21)) * RAD2DEG;
    *yaw = atan2f(r21, r11) * RAD2DEG;
    *roll = atan2f(r32, r33) * RAD2DEG;
}

Matrix ode_to_rl_transform(dBodyID _body) {
  // Get position and rotation directly from the body
  const dReal *pos = dBodyGetPosition(_body);
  const dReal *rot = dBodyGetRotation(_body);

  // Create rotation matrix (transposed from ODE's column-major to Raylib's row-major)
  Matrix rot_matrix = {
      (float)rot[0], (float)rot[1], (float)rot[2],  0.0f,
      (float)rot[4], (float)rot[5], (float)rot[6],  0.0f,
      (float)rot[8], (float)rot[9], (float)rot[10], 0.0f,
      0.0f,          0.0f,          0.0f,          1.0f
  };

  // Create translation matrix
  Matrix trans_matrix = MatrixTranslate((float)pos[0], (float)pos[1], (float)pos[2]);
  
  // Return the combined transformation
  return MatrixMultiply(rot_matrix, trans_matrix);
}

int main() {
    // Initialize ODE
    dInitODE();
    world = dWorldCreate();
    space = dHashSpaceCreate(0);
    contact_group = dJointGroupCreate(0);
    dWorldSetGravity(world, 0, -9.81f, 0);

    // Create ground plane
    ground = dCreatePlane(space, 0, 1, 0, 0);

    // Define cube size
    const float cube_size = 1.0f;

    // Create cube
    cube_body = dBodyCreate(world);
    dMass mass;
    dMassSetBox(&mass, 1.0, cube_size, cube_size, cube_size);
    dBodySetMass(cube_body, &mass);
    cube_geom = dCreateBox(space, cube_size, cube_size, cube_size);
    dGeomSetBody(cube_geom, cube_body);
    resetCubePosition(cube_body);

    // Initialize Raylib
    InitWindow(800, 600, "Cube Drop Simulation (ODE) - Press R to Reset");
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Model cube_model = LoadModelFromMesh(GenMeshCube(cube_size, cube_size, cube_size));
    SetRandomSeed((unsigned int)GetTime());

    // Main loop
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_R)) {
            resetCubePosition(cube_body);
        }

        dSpaceCollide(space, 0, &nearCallback);
        dWorldQuickStep(world, 1.0f / 60.0f);
        dJointGroupEmpty(contact_group);

        const dReal *pos = dBodyGetPosition(cube_body);
        const dReal *rot = dBodyGetRotation(cube_body);

        // // Log position and rotation to console
        // printf("ODE Pos: [%.2f, %.2f, %.2f]\n", pos[0], pos[1], pos[2]);
        // printf("ODE Rot: [%.2f, %.2f, %.2f, %.2f; %.2f, %.2f, %.2f, %.2f; %.2f, %.2f, %.2f, %.2f]\n",
        //        rot[0], rot[1], rot[2], rot[3],
        //        rot[4], rot[5], rot[6], rot[7],
        //        rot[8], rot[9], rot[10], rot[11]);

        // // correct
        // Matrix rot_matrix = {
        //     rot[0], rot[1], rot[2], 0,
        //     rot[4], rot[5], rot[6], 0,
        //     rot[8], rot[9], rot[10], 0,
        //     0, 0, 0, 1
        // };

        // // wrong rotate AI Model
        // Matrix rot_matrix = {
        //     rot[0], rot[4], rot[8], 0,
        //     rot[1], rot[5], rot[9], 0,
        //     rot[2], rot[6], rot[10], 0,
        //     0, 0, 0, 1
        // };
        
        // Matrix trans_matrix = MatrixTranslate((float)pos[0], (float)pos[1], (float)pos[2]);
        // cube_model.transform = MatrixMultiply(rot_matrix, trans_matrix);
        cube_model.transform = ode_to_rl_transform(cube_body);


        float yaw, pitch, roll;
        getYawPitchRoll(rot, &yaw, &pitch, &roll);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        DrawPlane((Vector3){0, 0, 0}, (Vector2){10, 10}, GRAY);
        DrawModel(cube_model, (Vector3){0, 0, 0}, 1.0f, RED);
        DrawModelWires(cube_model, (Vector3){0, 0, 0}, 1.0f, BLACK);

        EndMode3D();

        DrawFPS(10, 10);
        // Draw HUD using TextFormat
        DrawText("Press R to Randomize/Reset Cube", 10, 30, 20, DARKGRAY);
        DrawText(TextFormat("Position: X: %.2f  Y: %.2f  Z: %.2f", pos[0], pos[1], pos[2]), 10, 60, 20, DARKGRAY);
        DrawText(TextFormat("Rotation: Yaw: %.1f  Pitch: %.1f  Roll: %.1f", yaw, pitch, roll), 10, 90, 20, DARKGRAY);
        //DrawText(TextFormat("FPS: %i", GetFPS()), 10, 10, 20, DARKGRAY);  // Replacing DrawFPS with TextFormat version

        EndDrawing();
    }

    UnloadModel(cube_model);
    dJointGroupDestroy(contact_group);
    dSpaceDestroy(space);
    dWorldDestroy(world);
    dCloseODE();
    CloseWindow();

    return 0;
}