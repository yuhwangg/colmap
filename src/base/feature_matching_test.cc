// COLMAP - Structure-from-Motion and Multi-View Stereo.
// Copyright (C) 2016  Johannes L. Schoenberger <jsch at inf.ethz.ch>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE "base/feature_matching_test"
#include <boost/test/unit_test.hpp>

#include <QApplication>

#include "base/feature_matching.h"

using namespace colmap;

FeatureDescriptors CreateRandomFeatureDescriptors(const size_t num_features) {
  std::srand(0);
  const auto descriptors_float =
      L2NormalizeFeatureDescriptors(Eigen::MatrixXf::Random(num_features, 128) +
                                    Eigen::MatrixXf::Ones(num_features, 128));
  return FeatureDescriptorsToUnsignedByte(descriptors_float);
}

void CheckEqualMatches(const FeatureMatches& matches1,
                       const FeatureMatches& matches2) {
  BOOST_REQUIRE_EQUAL(matches1.size(), matches2.size());
  for (size_t i = 0; i < matches1.size(); ++i) {
    BOOST_CHECK_EQUAL(matches1[i].point2D_idx1, matches2[i].point2D_idx1);
    BOOST_CHECK_EQUAL(matches1[i].point2D_idx2, matches2[i].point2D_idx2);
  }
}

BOOST_AUTO_TEST_CASE(TestCreateSiftGPUMatcherOpenGL) {
  char app_name[] = "Test";
  int argc = 1;
  char* argv[] = {app_name};
  QApplication app(argc, argv);

  class TestThread : public Thread {
   private:
    void Run() {
      opengl_context_.MakeCurrent();
      SiftMatchGPU sift_match_gpu;
      BOOST_CHECK(CreateSiftGPUMatcher(SiftMatchOptions(), &sift_match_gpu));
    }
    OpenGLContextManager opengl_context_;
  };

  TestThread thread;
  RunThreadWithOpenGLContext(&app, &thread);
}

BOOST_AUTO_TEST_CASE(TestCreateSiftGPUMatcherCUDA) {
#ifdef CUDA_ENABLED
  SiftMatchGPU sift_match_gpu;
  SiftMatchOptions match_options;
  match_options.gpu_index = 0;
  BOOST_CHECK(CreateSiftGPUMatcher(match_options, &sift_match_gpu));
#endif
}

BOOST_AUTO_TEST_CASE(TestMatchSiftFeaturesCPU) {
  const FeatureDescriptors empty_descriptors =
      CreateRandomFeatureDescriptors(0);
  const FeatureDescriptors descriptors1 = CreateRandomFeatureDescriptors(2);
  const FeatureDescriptors descriptors2 = descriptors1.colwise().reverse();

  FeatureMatches matches;

  MatchSiftFeaturesCPU(SiftMatchOptions(), descriptors1, descriptors2,
                       &matches);
  BOOST_CHECK_EQUAL(matches.size(), 2);
  BOOST_CHECK_EQUAL(matches[0].point2D_idx1, 0);
  BOOST_CHECK_EQUAL(matches[0].point2D_idx2, 1);
  BOOST_CHECK_EQUAL(matches[1].point2D_idx1, 1);
  BOOST_CHECK_EQUAL(matches[1].point2D_idx2, 0);

  MatchSiftFeaturesCPU(SiftMatchOptions(), empty_descriptors, descriptors2,
                       &matches);
  BOOST_CHECK_EQUAL(matches.size(), 0);
  MatchSiftFeaturesCPU(SiftMatchOptions(), descriptors1, empty_descriptors,
                       &matches);
  BOOST_CHECK_EQUAL(matches.size(), 0);
  MatchSiftFeaturesCPU(SiftMatchOptions(), empty_descriptors, empty_descriptors,
                       &matches);
  BOOST_CHECK_EQUAL(matches.size(), 0);
}

BOOST_AUTO_TEST_CASE(TestMatchGuidedSiftFeaturesCPU) {
  FeatureKeypoints empty_keypoints(0);
  FeatureKeypoints keypoints1(2);
  keypoints1[0].x = 1;
  keypoints1[1].x = 2;
  FeatureKeypoints keypoints2(2);
  keypoints2[0].x = 2;
  keypoints2[1].x = 1;
  const FeatureDescriptors empty_descriptors =
      CreateRandomFeatureDescriptors(0);
  const FeatureDescriptors descriptors1 = CreateRandomFeatureDescriptors(2);
  const FeatureDescriptors descriptors2 = descriptors1.colwise().reverse();

  TwoViewGeometry two_view_geometry;
  two_view_geometry.config = TwoViewGeometry::PLANAR_OR_PANORAMIC;
  two_view_geometry.H = Eigen::Matrix3d::Identity();

  MatchGuidedSiftFeaturesCPU(SiftMatchOptions(), keypoints1, keypoints2,
                             descriptors1, descriptors2, &two_view_geometry);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 2);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 0);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 1);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx1, 1);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx2, 0);

  keypoints1[0].x = 100;
  MatchGuidedSiftFeaturesCPU(SiftMatchOptions(), keypoints1, keypoints2,
                             descriptors1, descriptors2, &two_view_geometry);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 1);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 1);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 0);

  MatchGuidedSiftFeaturesCPU(SiftMatchOptions(), empty_keypoints, keypoints2,
                             empty_descriptors, descriptors2,
                             &two_view_geometry);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 0);
  MatchGuidedSiftFeaturesCPU(SiftMatchOptions(), keypoints1, empty_keypoints,
                             descriptors1, empty_descriptors,
                             &two_view_geometry);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 0);
  MatchGuidedSiftFeaturesCPU(SiftMatchOptions(), empty_keypoints,
                             empty_keypoints, empty_descriptors,
                             empty_descriptors, &two_view_geometry);
  BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 0);
}

BOOST_AUTO_TEST_CASE(TestMatchSiftFeaturesGPU) {
  char app_name[] = "Test";
  int argc = 1;
  char* argv[] = {app_name};
  QApplication app(argc, argv);

  class TestThread : public Thread {
   private:
    void Run() {
      opengl_context_.MakeCurrent();
      SiftMatchGPU sift_match_gpu;
      BOOST_CHECK(CreateSiftGPUMatcher(SiftMatchOptions(), &sift_match_gpu));

      const FeatureDescriptors empty_descriptors =
          CreateRandomFeatureDescriptors(0);
      const FeatureDescriptors descriptors1 = CreateRandomFeatureDescriptors(2);
      const FeatureDescriptors descriptors2 = descriptors1.colwise().reverse();

      FeatureMatches matches;

      MatchSiftFeaturesGPU(SiftMatchOptions(), &descriptors1, &descriptors2,
                           &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 2);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx2, 0);

      MatchSiftFeaturesGPU(SiftMatchOptions(), nullptr, nullptr,
                           &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 2);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx2, 0);

      MatchSiftFeaturesGPU(SiftMatchOptions(), &descriptors1, nullptr,
                           &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 2);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx2, 0);

      MatchSiftFeaturesGPU(SiftMatchOptions(), nullptr, &descriptors2,
                           &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 2);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(matches[1].point2D_idx2, 0);

      MatchSiftFeaturesGPU(SiftMatchOptions(), &empty_descriptors,
                           &descriptors2, &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 0);
      MatchSiftFeaturesGPU(SiftMatchOptions(), &descriptors1,
                           &empty_descriptors, &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 0);
      MatchSiftFeaturesGPU(SiftMatchOptions(), &empty_descriptors,
                           &empty_descriptors, &sift_match_gpu, &matches);
      BOOST_CHECK_EQUAL(matches.size(), 0);
    }
    OpenGLContextManager opengl_context_;
  };

  TestThread thread;
  RunThreadWithOpenGLContext(&app, &thread);
}

BOOST_AUTO_TEST_CASE(TestMatchSiftFeaturesCPUvsGPU) {
  char app_name[] = "Test";
  int argc = 1;
  char* argv[] = {app_name};
  QApplication app(argc, argv);

  class TestThread : public Thread {
   private:
    void Run() {
      opengl_context_.MakeCurrent();
      SiftMatchGPU sift_match_gpu;
      BOOST_CHECK(CreateSiftGPUMatcher(SiftMatchOptions(), &sift_match_gpu));

      auto TestCPUvsGPU = [&sift_match_gpu](
          const SiftMatchOptions& options,
          const FeatureDescriptors& descriptors1,
          const FeatureDescriptors& descriptors2) {
        FeatureMatches matches_cpu;
        FeatureMatches matches_gpu;

        MatchSiftFeaturesCPU(options, descriptors1, descriptors2, &matches_cpu);
        MatchSiftFeaturesGPU(options, &descriptors1, &descriptors2,
                             &sift_match_gpu, &matches_gpu);
        CheckEqualMatches(matches_cpu, matches_gpu);

        const size_t num_matches = matches_cpu.size();

        const FeatureDescriptors empty_descriptors =
            CreateRandomFeatureDescriptors(0);

        MatchSiftFeaturesCPU(options, empty_descriptors, descriptors2,
                             &matches_cpu);
        MatchSiftFeaturesGPU(options, &empty_descriptors, &descriptors2,
                             &sift_match_gpu, &matches_gpu);
        CheckEqualMatches(matches_cpu, matches_gpu);

        MatchSiftFeaturesCPU(options, descriptors1, empty_descriptors,
                             &matches_cpu);
        MatchSiftFeaturesGPU(options, &descriptors1, &empty_descriptors,
                             &sift_match_gpu, &matches_gpu);
        CheckEqualMatches(matches_cpu, matches_gpu);

        MatchSiftFeaturesCPU(options, empty_descriptors, empty_descriptors,
                             &matches_cpu);
        MatchSiftFeaturesGPU(options, &empty_descriptors, &empty_descriptors,
                             &sift_match_gpu, &matches_gpu);
        CheckEqualMatches(matches_cpu, matches_gpu);

        return num_matches;
      };

      {
        const FeatureDescriptors descriptors1 =
            CreateRandomFeatureDescriptors(100);
        const FeatureDescriptors descriptors2 =
            CreateRandomFeatureDescriptors(100);
        SiftMatchOptions match_options;
        TestCPUvsGPU(match_options, descriptors1, descriptors2);
      }

      {
        const FeatureDescriptors descriptors1 =
            CreateRandomFeatureDescriptors(100);
        const FeatureDescriptors descriptors2 =
            descriptors1.colwise().reverse();
        SiftMatchOptions match_options;
        const size_t num_matches =
            TestCPUvsGPU(match_options, descriptors1, descriptors2);
        BOOST_CHECK_EQUAL(num_matches, 100);
      }

      // Check the ratio test.
      {
        FeatureDescriptors descriptors1 = CreateRandomFeatureDescriptors(100);
        FeatureDescriptors descriptors2 = descriptors1;

        SiftMatchOptions match_options;
        const size_t num_matches1 =
            TestCPUvsGPU(match_options, descriptors1, descriptors2);
        BOOST_CHECK_EQUAL(num_matches1, 100);

        descriptors2.row(99) = descriptors2.row(0);
        descriptors2(0, 0) += 50.0f;
        descriptors2.row(0) = FeatureDescriptorsToUnsignedByte(
            L2NormalizeFeatureDescriptors(descriptors2.row(0).cast<float>()));
        descriptors2(99, 0) += 100.0f;
        descriptors2.row(99) = FeatureDescriptorsToUnsignedByte(
            L2NormalizeFeatureDescriptors(descriptors2.row(99).cast<float>()));

        match_options.max_ratio = 0.4;
        const size_t num_matches2 =
            TestCPUvsGPU(match_options, descriptors1.topRows(99), descriptors2);
        BOOST_CHECK_EQUAL(num_matches2, 98);

        match_options.max_ratio = 0.5;
        const size_t num_matches3 =
            TestCPUvsGPU(match_options, descriptors1, descriptors2);
        BOOST_CHECK_EQUAL(num_matches3, 99);
      }

      // Check the cross check.
      {
        FeatureDescriptors descriptors1 = CreateRandomFeatureDescriptors(100);
        FeatureDescriptors descriptors2 = descriptors1;
        descriptors2.array() -= 1.0f;

        SiftMatchOptions match_options;

        match_options.cross_check = false;
        const size_t num_matches1 =
            TestCPUvsGPU(match_options, descriptors1, descriptors2);
        BOOST_CHECK_EQUAL(num_matches1, 87);

        match_options.cross_check = true;
        const size_t num_matches2 =
            TestCPUvsGPU(match_options, descriptors1, descriptors2);
        BOOST_CHECK_EQUAL(num_matches2, 67);
      }
    }
    OpenGLContextManager opengl_context_;
  };

  TestThread thread;
  RunThreadWithOpenGLContext(&app, &thread);
}

BOOST_AUTO_TEST_CASE(TestMatchGuidedSiftFeaturesGPU) {
  char app_name[] = "Test";
  int argc = 1;
  char* argv[] = {app_name};
  QApplication app(argc, argv);

  class TestThread : public Thread {
   private:
    void Run() {
      opengl_context_.MakeCurrent();
      SiftMatchGPU sift_match_gpu;
      BOOST_CHECK(CreateSiftGPUMatcher(SiftMatchOptions(), &sift_match_gpu));

      FeatureKeypoints empty_keypoints(0);
      FeatureKeypoints keypoints1(2);
      keypoints1[0].x = 1;
      keypoints1[1].x = 2;
      FeatureKeypoints keypoints2(2);
      keypoints2[0].x = 2;
      keypoints2[1].x = 1;
      const FeatureDescriptors empty_descriptors =
          CreateRandomFeatureDescriptors(0);
      const FeatureDescriptors descriptors1 = CreateRandomFeatureDescriptors(2);
      const FeatureDescriptors descriptors2 = descriptors1.colwise().reverse();

      TwoViewGeometry two_view_geometry;
      two_view_geometry.config = TwoViewGeometry::PLANAR_OR_PANORAMIC;
      two_view_geometry.H = Eigen::Matrix3d::Identity();

      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), &keypoints1, &keypoints2,
                                 &descriptors1, &descriptors2, &sift_match_gpu,
                                 &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 2);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx2, 0);

      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), nullptr, nullptr, nullptr,
                                 nullptr, &sift_match_gpu, &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 2);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx2, 0);

      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), &keypoints1, nullptr,
                                 &descriptors1, nullptr, &sift_match_gpu,
                                 &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 2);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx2, 0);

      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), nullptr, &keypoints2,
                                 nullptr, &descriptors2, &sift_match_gpu,
                                 &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 2);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 0);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[1].point2D_idx2, 0);

      keypoints1[0].x = 100;
      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), &keypoints1, &keypoints2,
                                 &descriptors1, &descriptors2, &sift_match_gpu,
                                 &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx1, 1);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches[0].point2D_idx2, 0);

      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), &empty_keypoints,
                                 &keypoints2, &empty_descriptors, &descriptors2,
                                 &sift_match_gpu, &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 0);
      MatchGuidedSiftFeaturesGPU(
          SiftMatchOptions(), &keypoints1, &empty_keypoints, &descriptors1,
          &empty_descriptors, &sift_match_gpu, &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 0);
      MatchGuidedSiftFeaturesGPU(SiftMatchOptions(), &empty_keypoints,
                                 &empty_keypoints, &empty_descriptors,
                                 &empty_descriptors, &sift_match_gpu,
                                 &two_view_geometry);
      BOOST_CHECK_EQUAL(two_view_geometry.inlier_matches.size(), 0);
    }
    OpenGLContextManager opengl_context_;
  };

  TestThread thread;
  RunThreadWithOpenGLContext(&app, &thread);
}
