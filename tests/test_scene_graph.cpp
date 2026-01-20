#include <gtest/gtest.h>
#include "../src/Core/Scene.h"
#include "../src/Core/HierarchySystem.h"
#include "../src/Core/SceneSerializer.h"
#include "../src/Bridge/ResourceLoader.h"
#include <cstdio>
#include <fstream>

using namespace Nexus;

class SceneGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        ResourceLoader::setBasePath("./");
    }
};

TEST_F(SceneGraphTest, HierarchySetupAndDestroy) {
    Scene scene("TestScene");

    Entity root = scene.createEntity("Root");
    Entity child1 = scene.createEntity("Child1");
    Entity child2 = scene.createEntity("Child2");
    Entity grandchild = scene.createEntity("Grandchild");

    scene.setParent(child1, root);
    scene.setParent(child2, root);
    scene.setParent(grandchild, child1);

    auto& rootHier = root.getComponent<HierarchyComponent>();
    EXPECT_EQ(rootHier.children.size(), 2);
    EXPECT_EQ(rootHier.children[0], child1.getHandle());
    EXPECT_TRUE(rootHier.parent == entt::null);

    auto& child1Hier = child1.getComponent<HierarchyComponent>();
    EXPECT_EQ(child1Hier.parent, root.getHandle());
    EXPECT_EQ(child1Hier.children.size(), 1);

    scene.destroyEntity(root);

    EXPECT_FALSE(scene.getRegistry().getInternal().valid(root.getHandle()));
    EXPECT_FALSE(scene.getRegistry().getInternal().valid(child1.getHandle()));
    EXPECT_FALSE(scene.getRegistry().getInternal().valid(child2.getHandle()));
    EXPECT_FALSE(scene.getRegistry().getInternal().valid(grandchild.getHandle()));
}

TEST_F(SceneGraphTest, TransformPropagation) {
    Scene scene("TestScene");
    Entity root = scene.createEntity("Root");
    Entity child = scene.createEntity("Child");
    scene.setParent(child, root);

    auto& rootTrans = root.getComponent<TransformComponent>();
    rootTrans.position = {10.0f, 0.0f, 0.0f};

    auto& childTrans = child.getComponent<TransformComponent>();
    childTrans.position = {5.0f, 0.0f, 0.0f};

    EXPECT_EQ(childTrans.worldMatrix[12], 0.0f);

    HierarchySystem::update(scene.getRegistry());

    EXPECT_FLOAT_EQ(rootTrans.worldMatrix[12], 10.0f);

    EXPECT_FLOAT_EQ(childTrans.worldMatrix[12], 15.0f);
}

TEST_F(SceneGraphTest, SceneSerialization) {
    std::string testFile = "test_scene.bin";

    {
        Scene scene("SaveScene");
        Entity e1 = scene.createEntity("RootEnt");
        e1.getComponent<TransformComponent>().position = {1.0f, 2.0f, 3.0f};

        Entity e2 = scene.createEntity("ChildEnt");
        scene.setParent(e2, e1);

        SceneSerializer serializer(scene);
        EXPECT_TRUE(serializer.serialize(testFile));
    }

    {
        Scene loadedScene("LoadScene");
        SceneSerializer deserializer(loadedScene);
        EXPECT_TRUE(deserializer.deserialize(testFile));

        auto& reg = loadedScene.getRegistry().getInternal();
        EXPECT_EQ(reg.storage<entt::entity>().size(), 2);

        Entity loadedRoot, loadedChild;
        for (auto entity : reg.storage<entt::entity>()) {
            Entity e(entity, &loadedScene.getRegistry());
            if (e.getComponent<TagComponent>().name == "RootEnt") {
                loadedRoot = e;
            } else if (e.getComponent<TagComponent>().name == "ChildEnt") {
                loadedChild = e;
            }
        }

        EXPECT_TRUE(loadedRoot.isValid());
        EXPECT_TRUE(loadedChild.isValid());

        auto& rootTrans = loadedRoot.getComponent<TransformComponent>();
        EXPECT_FLOAT_EQ(rootTrans.position[0], 1.0f);
        EXPECT_FLOAT_EQ(rootTrans.position[1], 2.0f);
        EXPECT_FLOAT_EQ(rootTrans.position[2], 3.0f);

        auto& rootHier = loadedRoot.getComponent<HierarchyComponent>();
        EXPECT_EQ(rootHier.children.size(), 1);
        EXPECT_EQ(rootHier.children[0], loadedChild.getHandle());

        auto& childHier = loadedChild.getComponent<HierarchyComponent>();
        EXPECT_EQ(childHier.parent, loadedRoot.getHandle());
    }

    std::remove(testFile.c_str());
}
