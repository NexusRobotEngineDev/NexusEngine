#include <gtest/gtest.h>
#include "MuJoCo/MuJoCo_PhysicsSystem.h"
#include "Log.h"

using namespace Nexus;

class MuJoCoPhysicsTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(MuJoCoPhysicsTest, Initialization) {
    MuJoCo_PhysicsSystem physics;
    auto status = physics.initialize();
    EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(MuJoCoPhysicsTest, StepSimulation) {
    MuJoCo_PhysicsSystem physics;
    ASSERT_TRUE(physics.initialize().ok());

    for (int i = 0; i < 10; ++i) {
        physics.update(1.0f / 60.0f);
    }

    physics.shutdown();
}
