/*=========================================================================

Library:   TubeTK

Copyright 2010 Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved.

Licensed under the Apache License, Version 2.0 ( the "License" );
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

=========================================================================*/
#ifndef __tubeComputeTrainingMask_hxx
#define __tubeComputeTrainingMask_hxx

#include "tubeComputeTrainingMask.h"

namespace tube {

/**
 *
 */
template< class TImage >
ComputeTrainingMask< TImage >
::ComputeTrainingMask( void )
{
  m_ComputeTrainingMaskFilter = FilterType::New();
}


/**
 *
 */
template< class TImage >
void
ComputeTrainingMask< TImage >
::PrintSelf( std::ostream & os, itk::Indent  indent ) const
{
  os << indent << "Gap:"
     << m_ComputeTrainingMaskFilter->GetGap() << std::endl;
  os << indent << "NotVesselWidth:"
     << m_ComputeTrainingMaskFilter->GetNotVesselWidth() << std::endl;
}


} // end namespace tube

#endif
