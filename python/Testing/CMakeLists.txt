##############################################################################
#
# Library:   TubeTK
#
# Copyright 2010 Kitware Inc. 28 Corporate Drive,
# Clifton Park, NY, 12065, USA.
#
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
##############################################################################

set( TEMP ${TubeTK_BINARY_DIR}/Temporary )

ExternalData_Add_Test_With_Env(
  NAME Python.VesselTubeToNumPyTest
  COMMAND ${TubeTK_LAUNCHER} ${PYTHON_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tubetk.py
    VesselTubeToNumPyTest
      DATA{${TubeTK_DATA_ROOT}/tube.tre}
      DATA{${TubeTK_DATA_ROOT}/tube.tre.npy}
  ENVIRONMENT TubeTK_BUILD_DIR=${TubeTK_BINARY_DIR}
  )
