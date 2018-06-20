// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "android/base/files/PathUtils.h"
#include "android/base/system/System.h"
#include "android/base/testing/TestSystem.h"

#include <gtest/gtest.h>

#include "OpenGLTestContext.h"

#include <memory>

namespace emugl {

struct GlValues {
    std::vector<GLint> ints;
    std::vector<GLfloat> floats;
};

struct GlSampleCoverage {
    GLclampf value;
    GLboolean invert;
};

struct GlStencilFunc {
    GLenum func;
    GLint ref;
    GLuint mask;
};

struct GlStencilOp {
    GLenum sfail;
    GLenum dpfail;
    GLenum dppass;
};

// Capabilities which, according to the GLES2 spec, start disabled.
static const GLenum kGLES2CanBeEnabled[] = {GL_BLEND,
                                            GL_CULL_FACE,
                                            GL_DEPTH_TEST,
                                            GL_POLYGON_OFFSET_FILL,
                                            GL_SAMPLE_ALPHA_TO_COVERAGE,
                                            GL_SAMPLE_COVERAGE,
                                            GL_SCISSOR_TEST,
                                            GL_STENCIL_TEST};

// Capabilities which, according to the GLES2 spec, start enabled.
static const GLenum kGLES2CanBeDisabled[] = {GL_DITHER};

// Modes for CullFace
static const GLenum kGLES2CullFaceModes[] = {GL_BACK, GL_FRONT,
                                             GL_FRONT_AND_BACK};

// Modes for FrontFace
static const GLenum kGLES2FrontFaceModes[] = {GL_CCW, GL_CW};

// Valid Stencil test functions
static const GLenum kGLES2StencilFuncs[] = {GL_NEVER,   GL_ALWAYS,  GL_LESS,
                                            GL_LEQUAL,  GL_EQUAL,   GL_GEQUAL,
                                            GL_GREATER, GL_NOTEQUAL};
// Valid Stencil test result operations
static const GLenum kGLES2StencilOps[] = {GL_KEEP,      GL_ZERO,     GL_REPLACE,
                                          GL_INCR,      GL_DECR,     GL_INVERT,
                                          GL_INCR_WRAP, GL_DECR_WRAP};

// SnapshotTest - A helper class for performing a test related to saving or
// loading GL translator snapshots. As a test fixture, its setup will prepare a
// fresh GL state and paths for temporary snapshot files.
//
// doSnapshot saves a snapshot, clears the GL state, then loads the snapshot.
// saveSnapshot and loadSnapshot can be used to perform saves and loads
// independently.
//
// Usage example:
//     TEST_F(SnapshotTest, PreserveFooBar) {
//         // clean GL state is ready
//         EXPECT_TRUE(fooBarState());
//         modifyGlStateFooBar();
//         EXPECT_FALSE(fooBarState());  // GL state has been changed
//         doSnapshot(); // saves, resets, and reloads the state
//         EXPECT_FALSE(fooBarState());  // Snapshot preserved the state change
//     }
//
class SnapshotTest : public gltest::GLTest {
public:
    SnapshotTest()
        : mTestSystem(PATH_SEP "progdir",
                      android::base::System::kProgramBitness,
                      PATH_SEP "homedir",
                      PATH_SEP "appdir") {}

    void SetUp() override;

    // Mimics FrameBuffer.onSave, with fewer objects to manage.
    // |streamFile| is a filename into which the snapshot will be saved.
    // |textureFile| is a filename into which the textures will be saved.
    void saveSnapshot(const std::string streamFile,
                      const std::string textureFile);

    // Mimics FrameBuffer.onLoad, with fewer objects to manage.
    // Assumes that a valid display is present.
    // |streamFile| is a filename from which the snapshot will be loaded.
    // |textureFile| is a filename from which the textures will be loaded.
    void loadSnapshot(const std::string streamFile,
                      const std::string textureFile);

    // Performs a teardown and reset of graphics objects in preparation for
    // a snapshot load.
    void preloadReset();

    // Mimics saving and then loading a graphics snapshot.
    // To verify that state has been reset to some default before the load,
    // assertions can be performed in |preloadCheck|.
    void doSnapshot(std::function<void()> preloadCheck);

protected:
    android::base::TestSystem mTestSystem;
    std::string mSnapshotPath = {};
};

// SnapshotPreserveTest - A helper class building on SnapshotTest for granular
// testing of the GL snapshot. This is specifically for the common case where a
// piece of GL state has a known default, and our test aims to verify that the
// snapshot preserves this piece of state when it has been changed from the
// default.
//
// This acts as an abstract class; implementations should override the state
// check state change functions to perform the assertions and operations
// relevant to the part of GL state that they are testing.
// doCheckedSnapshot can be but does not need to be overwritten. It performs the
// following:
//      - check for default state
//      - make state changes, check that the state changes are in effect
//      - save a snapshot, reset the GL state, then check for default state
//      - load the snapshot, check that the state changes are in effect again
//
// Usage example with a subclass:
//     class SnapshotEnableFooTest : public SnapshotPreserveTest {
//         void defaultStateCheck() override { EXPECT_FALSE(isFooEnabled()); }
//         void changedStateCheck() override { EXPECT_TRUE(isFooEnabled());  }
//         void stateChange() override { enableFoo(); }
//     };
//     TEST_F(SnapshotEnableFooTest, PreserveFooEnable) {
//         doCheckedSnapshot();
//     }
//
class SnapshotPreserveTest : public SnapshotTest {
public:
    // Asserts that we are working from a clean starting state.
    virtual void defaultStateCheck() {
        ADD_FAILURE() << "Snapshot test needs a default state check function.";
    }

    // Asserts that any expected changes to state have occurred.
    virtual void changedStateCheck() {
        ADD_FAILURE()
                << "Snapshot test needs a post-change state check function.";
    }

    // Modifies the state.
    virtual void stateChange() {
        ADD_FAILURE() << "Snapshot test needs a state-changer function.";
    }

    // Sets up a non-default state and asserts that a snapshot preserves it.
    virtual void doCheckedSnapshot();
};

// SnapshotSetValueTest - A helper class for testing preservation of pieces of
// GL state where default and changed state checks are comparisons against the
// same type of expected reference value.
//
// The expected |m_default_value| and |m_changed_value| should be set before
// a checked snapshot is attempted.
//
// Usage example with a subclass:
//     class SnapshotSetFooTest : public SnapshotSetValueTest<Foo> {
//         void stateCheck(Foo expected) { EXPECT_EQ(expected, getFoo()); }
//         void stateChange() override { setFoo(*m_changed_value); }
//     };
//     TEST_F(SnapshotSetFooTest, SetFooValue) {
//         setExpectedValues(kFooDefaultValue, kFooTestValue);
//         doCheckedSnapshot();
//     }
//
template <class T>
class SnapshotSetValueTest : public SnapshotPreserveTest {
public:
    // Configures the test to assert against values which it should consider
    // default and values which it should expect after changes.
    void setExpectedValues(T defaultValue, T changedValue) {
        m_default_value = std::unique_ptr<T>(new T(defaultValue));
        m_changed_value = std::unique_ptr<T>(new T(changedValue));
    }

    // Checks part of state against an expected value.
    virtual void stateCheck(T expected) {
        ADD_FAILURE() << "Snapshot test needs a state-check function.";
    };

    void defaultStateCheck() override { stateCheck(*m_default_value); }
    void changedStateCheck() override { stateCheck(*m_changed_value); }

    void doCheckedSnapshot() override {
        if (m_default_value == nullptr || m_changed_value == nullptr) {
            FAIL() << "Snapshot test not provided expected values.";
        }
        SnapshotPreserveTest::doCheckedSnapshot();
    }

protected:
    std::unique_ptr<T> m_default_value;
    std::unique_ptr<T> m_changed_value;
};

}  // namespace emugl
