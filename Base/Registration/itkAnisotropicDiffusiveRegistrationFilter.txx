/*=========================================================================

Library:   TubeTK

Copyright 2010 Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

=========================================================================*/
#ifndef __itkAnisotropicDiffusiveRegistrationFilter_txx
#define __itkAnisotropicDiffusiveRegistrationFilter_txx

#include "itkAnisotropicDiffusiveRegistrationFilter.h"
#include "itkSmoothingRecursiveGaussianImageFilter.h"

#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "vtkPointLocator.h"
#include "vtkPolyData.h"
#include "vtkPolyDataNormals.h"

namespace itk
{

/**
 * Constructor
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
AnisotropicDiffusiveRegistrationFilter
< TFixedImage, TMovingImage, TDeformationField >
::AnisotropicDiffusiveRegistrationFilter()
{
  // Initialize attributes to NULL
  m_BorderSurface                               = 0;
  m_NormalVectorImage                           = 0;
  m_WeightImage                                 = 0;
  m_HighResolutionNormalVectorImage             = 0;
  m_HighResolutionWeightImage                   = 0;

  // Lambda for exponential decay used to calculate weight from distance
  m_Lambda = -0.01;
}

/**
 * PrintSelf
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::PrintSelf( std::ostream& os, Indent indent ) const
{
  Superclass::PrintSelf( os, indent );
  if( m_BorderSurface )
    {
    os << indent << "Border surface:" << std::endl;
    m_BorderSurface->Print( os );
    }
  if( m_NormalVectorImage )
    {
    os << indent << "Normal vector image:" << std::endl;
    m_NormalVectorImage->Print( os, indent );
    }
  if( m_WeightImage )
    {
    os << indent << "Weight image:" << std::endl;
    m_WeightImage->Print( os, indent );
    }
  os << indent << "lambda: " << m_Lambda << std::endl;
  if( m_HighResolutionNormalVectorImage )
    {
    os << indent << "High resolution normal vector image:" << std::endl;
    m_HighResolutionNormalVectorImage->Print( os, indent );
    }
  if( m_HighResolutionWeightImage )
    {
    os << indent << "High resolution weight image:" << std::endl;
    m_HighResolutionWeightImage->Print( os, indent );
    }
}

/**
 * Setup the pointers for the deformation component images
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::InitializeDeformationComponentAndDerivativeImages()
{
  assert( this->GetComputeRegularizationTerm() );
  assert( this->GetOutput() );

  // The output will be used as the template to allocate the images we will
  // use to store data computed before/during the registration
  OutputImagePointer output = this->GetOutput();

  // Setup pointers to the deformation component images - we have the
  // TANGENTIAL component, which is the entire deformation field, and the
  // NORMAL component, which is the deformation vectors projected onto their
  // normals
  this->SetDeformationComponentImage( TANGENTIAL, this->GetOutput() );

  DeformationFieldPointer normalDeformationField = DeformationFieldType::New();
  this->AllocateSpaceForImage( normalDeformationField, output );
  this->SetDeformationComponentImage( NORMAL, normalDeformationField );

  // Setup the first and second order deformation component images - we need
  // to allocate images for both the TANGENTIAL and NORMAL components, so
  // we'll just loop over the number of terms
  ScalarDerivativeImagePointer firstOrder;
  TensorDerivativeImagePointer secondOrder;
  for( int i = 0; i < this->GetNumberOfTerms(); i++ )
    {
    for( int j = 0; j < ImageDimension; j++ )
      {
      firstOrder = ScalarDerivativeImageType::New();
      this->AllocateSpaceForImage( firstOrder, output );
      secondOrder = TensorDerivativeImageType::New();
      this->AllocateSpaceForImage( secondOrder, output );
      this->SetDeformationComponentFirstOrderDerivative( i, j, firstOrder );
      this->SetDeformationComponentSecondOrderDerivative( i, j, secondOrder );
      }
    }

  // If required, allocate and compute the normal vector and weight images
  this->SetupNormalVectorAndWeightImages();
}

/**
 * All other initialization done before the initialize / calculate change /
 * apply update loop
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::SetupNormalVectorAndWeightImages()
{
  assert( this->GetComputeRegularizationTerm() );
  assert( this->GetOutput() );

  // Whether or not we must compute the normal vector and/or weight images
  bool computeNormals = !m_NormalVectorImage;
  bool computeWeights = !m_WeightImage;

  // If we have a template for image attributes, use it.  The normal and weight
  // images will be stored at their full resolution.  The diffusion tensor,
  // deformation component, derivative and multiplication vector images are
  // recalculated every time iterate() is called to regenerate them at the
  // correct resolution.
  FixedImagePointer highResolutionTemplate = this->GetHighResolutionTemplate();

  // If we don't have a template:
  // The output will be used as the template to allocate the images we will
  // use to store data computed before/during the registration
  OutputImagePointer output = this->GetOutput();

  // Compute the normal vector and/or weight images if required
  if( computeNormals || computeWeights )
    {
    // Ensure we have a border surface to work with
    if( !this->GetBorderSurface() )
      {
      itkExceptionMacro( << "You must provide a border surface, or both a "
                         << "normal vector image and a weight image" );
      }

    // Compute the normals for the surface
    this->ComputeBorderSurfaceNormals();

    // Allocate the normal vector and/or weight images
    if( computeNormals )
      {
      m_NormalVectorImage = NormalVectorImageType::New();
      if( highResolutionTemplate )
        {
        this->AllocateSpaceForImage( m_NormalVectorImage,
                                     highResolutionTemplate );
        }
      else
        {
        this->AllocateSpaceForImage( m_NormalVectorImage, output );
        }
      }
    if( computeWeights )
      {
      m_WeightImage = WeightImageType::New();
      if( highResolutionTemplate )
        {
        this->AllocateSpaceForImage( m_WeightImage, highResolutionTemplate );
        }
      else
        {
        this->AllocateSpaceForImage( m_WeightImage, output );
        }
      }

    // Actually compute the normal vectors and/or weights
    this->ComputeNormalVectorAndWeightImages( computeNormals, computeWeights );
    }

  // If we are using a template or getting an image from the user, we need to
  // make sure that the attributes of the member images match those of the
  // current output, so that they can be used to calclulate the diffusion
  // tensors, deformation components, etc
  if( !this->CompareImageAttributes( m_NormalVectorImage, output ) )
    {
    // Set the high resolution image only once
    if( !m_HighResolutionNormalVectorImage )
      {
      m_HighResolutionNormalVectorImage = m_NormalVectorImage;
      }
    this->ResampleImageNearestNeighbor(
        m_HighResolutionNormalVectorImage.GetPointer(),
        output.GetPointer(),
        m_NormalVectorImage.GetPointer() );
    }
  if( !this->CompareImageAttributes( m_WeightImage, output ) )
    {
    // Set the high resolution image only once
    if( !m_HighResolutionWeightImage )
      {
      m_HighResolutionWeightImage = m_WeightImage;
      }
    this->ResampleImageLinear( m_HighResolutionWeightImage.GetPointer(),
                               output.GetPointer(),
                               m_WeightImage.GetPointer() );
    }
}

/**
 * Compute the normals for the border surface
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::ComputeBorderSurfaceNormals()
{
  assert( m_BorderSurface );
  vtkPolyDataNormals * normalsFilter = vtkPolyDataNormals::New();
  normalsFilter->ComputePointNormalsOn();
  normalsFilter->ComputeCellNormalsOff();
  //normalsFilter->SetFeatureAngle(30); // TODO
  normalsFilter->SetInput( m_BorderSurface );
  normalsFilter->Update();
  m_BorderSurface = normalsFilter->GetOutput();
  normalsFilter->Delete();

  // Make sure we now have the normals
  if ( !m_BorderSurface->GetPointData() )
    {
    itkExceptionMacro( << "Border surface does not contain point data" );
    }
  else if ( !m_BorderSurface->GetPointData()->GetNormals() )
    {
    itkExceptionMacro( << "Border surface point data does not have normals" );
    }
}

/**
 * Computes the normal vectors and distances to the closest point
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::GetNormalsAndDistancesFromClosestSurfacePoint( bool computeNormals,
                                                 bool computeWeights )
{
  // Setup the point locator and get the normals from the polydata
  vtkPointLocator * pointLocator = vtkPointLocator::New();
  pointLocator->SetDataSet( m_BorderSurface );
  pointLocator->Initialize();
  pointLocator->BuildLocator();
  vtkFloatArray * normalData
      = static_cast< vtkFloatArray * >
        (m_BorderSurface->GetPointData()->GetNormals() );

  // Set up struct for multithreaded processing.
  AnisotropicDiffusiveRegistrationFilterThreadStruct str;
  str.Filter = this;
  str.PointLocator = pointLocator;
  str.NormalData = normalData;
  str.ComputeNormals = computeNormals;
  str.ComputeWeights = computeWeights;

  // Multithread the execution
  this->GetMultiThreader()->SetNumberOfThreads( this->GetNumberOfThreads() );
  this->GetMultiThreader()->SetSingleMethod(
      this->GetNormalsAndDistancesFromClosestSurfacePointThreaderCallback,
      & str );
  this->GetMultiThreader()->SingleMethodExecute();

  // Explicitly call Modified on the normal and weight images here, since
  // ThreadedGetNormalsAndDistancesFromClosestSurfacePoint changes these buffers
  // through iterators which do not increment the update buffer timestamp
  this->m_NormalVectorImage->Modified();
  this->m_WeightImage->Modified();

  // Clean up memory
  pointLocator->Delete();
}

/**
 * Calls ThreadedGetNormalsAndDistancesFromClosestSurfacePoint for processing
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
ITK_THREAD_RETURN_TYPE
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::GetNormalsAndDistancesFromClosestSurfacePointThreaderCallback( void * arg )
{
  int threadId = ((MultiThreader::ThreadInfoStruct *)(arg))->ThreadID;
  int threadCount = ((MultiThreader::ThreadInfoStruct *)(arg))->NumberOfThreads;

  AnisotropicDiffusiveRegistrationFilterThreadStruct * str
      = (AnisotropicDiffusiveRegistrationFilterThreadStruct *)
            (((MultiThreader::ThreadInfoStruct *)(arg))->UserData);

  // Execute the actual method with appropriate output region
  // first find out how many pieces extent can be split into.
  // Using the SplitRequestedRegion method from itk::ImageSource.
  int total;
  ThreadNormalVectorImageRegionType splitNormalRegion;
  total = str->Filter->SplitRequestedRegion( threadId, threadCount,
                                             splitNormalRegion );

  ThreadWeightImageRegionType splitWeightRegion;
  total = str->Filter->SplitRequestedRegion( threadId, threadCount,
                                             splitWeightRegion );

  if( threadId < total )
    {
    str->Filter->ThreadedGetNormalsAndDistancesFromClosestSurfacePoint(
        str->PointLocator,
        str->NormalData,
        splitNormalRegion,
        splitWeightRegion,
        str->ComputeNormals,
        str->ComputeWeights,
        threadId );
    }

  return ITK_THREAD_RETURN_VALUE;
}

/**
 * Does the actual work of computing the normal vectors and distances to the
 * closest point given an initialized vtkPointLocator and the surface border
 * normals
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::ThreadedGetNormalsAndDistancesFromClosestSurfacePoint(
    vtkPointLocator * pointLocator,
    vtkFloatArray * normalData,
    ThreadNormalVectorImageRegionType & normalRegionToProcess,
    ThreadWeightImageRegionType & weightRegionToProcess,
    bool computeNormals,
    bool computeWeights,
    int )
{
  // Setup iterators over the normal vector and weight images
  NormalVectorImageRegionType normalIt( m_NormalVectorImage,
                                        normalRegionToProcess );
  WeightImageRegionType weightIt(m_WeightImage,
                                 weightRegionToProcess );

  // The normal vector image will hold the normal of the closest point of the
  // surface polydata, and the weight image will be a function of the distance
  // between the voxel and this closest point

  itk::Point< double, ImageDimension >  imageCoord;
  imageCoord.Fill( 0 );
  double                                borderCoord[ImageDimension];
  for( unsigned int i = 0; i < ImageDimension; i++ )
    {
    borderCoord[i] = 0.0;
    }
  vtkIdType                             id = 0;
  WeightType                            distance = 0;
  NormalVectorType                      normal;
  normal.Fill(0);

  // Determine the normals of and the distances to the nearest border point
  for( normalIt.GoToBegin(), weightIt.GoToBegin();
       !normalIt.IsAtEnd();
       ++normalIt, ++weightIt )
    {
    // Find the id of the closest surface point to the current voxel
    m_NormalVectorImage->TransformIndexToPhysicalPoint( normalIt.GetIndex(),
                                                        imageCoord );
    id = pointLocator->FindClosestPoint( imageCoord.GetDataPointer() );

    // Find the normal of the surface point that is closest to the current voxel
    if( computeNormals )
      {
      for( unsigned int i = 0; i < ImageDimension; i++ )
        {
        normal[i] = normalData->GetValue( id * ImageDimension + i );
        }
      normalIt.Set( normal );
      }

    // Calculate distance between the current coordinate and the border surface
    // coordinate
    if( computeWeights )
      {
      m_BorderSurface->GetPoint( id, borderCoord );
      distance = 0.0;
      for( unsigned int i = 0; i < ImageDimension; i++ )
        {
        distance += pow( imageCoord[i] - borderCoord[i], 2 );
        }
      distance = sqrt( distance );
      // The weight image will temporarily store distances
      weightIt.Set( distance );
      }
    }
}

/**
 * Updates the border normals and the weighting factor w
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::ComputeNormalVectorAndWeightImages( bool computeNormals, bool computeWeights )
{
  assert( this->GetComputeRegularizationTerm() );
  assert( m_BorderSurface->GetPointData()->GetNormals() );
  assert( m_NormalVectorImage );
  assert( m_WeightImage );

  std::cout << "Computing normals and weights... " << std::endl;

  // The normal vector image will hold the normal of the closest point of the
  // surface polydata, and the weight image will be a function of the distance
  // between the voxel and this closest point
  this->GetNormalsAndDistancesFromClosestSurfacePoint( computeNormals,
                                                       computeWeights );

  // Smooth the normals to handle corners (because we are choosing the
  // closest point in the polydata
  if( computeNormals )
    {
    //  typedef itk::RecursiveGaussianImageFilter
    //      < NormalVectorImageType, NormalVectorImageType >
    //      NormalSmoothingFilterType;
    //  typename NormalSmoothingFilterType::Pointer normalSmooth
    //      = NormalSmoothingFilterType::New();
    //  normalSmooth->SetInput( m_NormalVectorImage );
    //  double normalSigma = 3.0;
    //  normalSmooth->SetSigma( normalSigma );
    //  normalSmooth->Update();
    //  m_NormalVectorImage = normalSmooth->GetOutput();
    }

  // Smooth the distance image to avoid "streaks" from faces of the polydata
  // (because we are choosing the closest point in the polydata)
  if( computeWeights )
    {
    double weightSmoothingSigma = 1.0;
    typedef itk::SmoothingRecursiveGaussianImageFilter
        < WeightImageType, WeightImageType > WeightSmoothingFilterType;
    typename WeightSmoothingFilterType::Pointer weightSmooth
        = WeightSmoothingFilterType::New();
    weightSmooth->SetInput( m_WeightImage );
    weightSmooth->SetSigma( weightSmoothingSigma );
    weightSmooth->Update();
    m_WeightImage = weightSmooth->GetOutput();

    // Iterate through the weight image and compute the weight from the
    WeightType weight = 0;
    WeightImageRegionType weightIt(
        m_WeightImage, m_WeightImage->GetLargestPossibleRegion() );
    for( weightIt.GoToBegin(); !weightIt.IsAtEnd(); ++weightIt )
      {
      weight = this->ComputeWeightFromDistance( weightIt.Get() );
      weightIt.Set( weight );
      }
    }

  std::cout << "Finished computing normals and weights." << std::endl;
}

/**
 * Calculates the weighting between the anisotropic diffusive and diffusive
 * regularizations, based on a given distance from a voxel to the border
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
typename AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::WeightType
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::ComputeWeightFromDistance( const WeightType distance ) const
{
  return exp( m_Lambda * distance );
}

/**
 * Updates the diffusion tensor image before each run of the registration
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::ComputeDiffusionTensorImages()
{
  assert( this->GetComputeRegularizationTerm() );
  assert( m_NormalVectorImage );
  assert( m_WeightImage );

  // For the anisotropic diffusive regularization, we need to setup the
  // tangential diffusion tensors and the normal diffusion tensors

  // Used to compute the tangential and normal diffusion tensor images:
  // P = I - wnn^T
  // tangentialMatrix = tangentialDiffusionTensor = P^TP
  // normalMatrix = normalDiffusionTensor = w^2nn^T

  typedef itk::ImageRegionIterator< DiffusionTensorImageType >
      DiffusionTensorImageRegionType;
  typedef itk::Matrix
      < DeformationVectorComponentType, ImageDimension, ImageDimension >
      MatrixType;

  NormalVectorType       n;
  WeightType             w;
  MatrixType             P;
  MatrixType             normalMatrix;
  MatrixType             tangentialMatrix;
  DiffusionTensorType    tangentialDiffusionTensor;
  DiffusionTensorType    normalDiffusionTensor;

  // Setup iterators
  NormalVectorImageRegionType normalIt = NormalVectorImageRegionType(
      m_NormalVectorImage, m_NormalVectorImage->GetLargestPossibleRegion() );
  WeightImageRegionType weightIt = WeightImageRegionType(
      m_WeightImage, m_WeightImage->GetLargestPossibleRegion() );
  DiffusionTensorImageRegionType tangentialTensorIt
      = DiffusionTensorImageRegionType(
          this->GetDiffusionTensorImage( TANGENTIAL ),
          this->GetDiffusionTensorImage( TANGENTIAL )
          ->GetLargestPossibleRegion() );
  DiffusionTensorImageRegionType normalTensorIt
      = DiffusionTensorImageRegionType(
          this->GetDiffusionTensorImage( NORMAL ),
          this->GetDiffusionTensorImage( NORMAL )->GetLargestPossibleRegion() );

  for( normalIt.GoToBegin(), weightIt.GoToBegin(),
       tangentialTensorIt.GoToBegin(), normalTensorIt.GoToBegin();
       !tangentialTensorIt.IsAtEnd();
       ++normalIt, ++weightIt, ++tangentialTensorIt, ++normalTensorIt )
    {
    n = normalIt.Get();
    w = weightIt.Get();

    // The matrices are used for calculations, and will be copied to the
    // diffusion tensors afterwards.  The matrices are guaranteed to be
    // symmetric.

    // Create the normalMatrix used to calculate nn^T
    // (The first column is filled with the values of n, the rest are 0s)
    for ( unsigned int i = 0; i < ImageDimension; i++ )
      {
      normalMatrix( i, 0 ) = n[i];
      for ( unsigned int j = 1; j < ImageDimension; j++ )
        {
        normalMatrix( i, j ) = 0;
        }
      }

    // Calculate the normal and tangential diffusion tensors
    normalMatrix = normalMatrix * normalMatrix.GetTranspose(); // nn^T
    normalMatrix = normalMatrix * w; // wnn^T
    P.SetIdentity();
    P = P - normalMatrix; // I - wnn^T
    tangentialMatrix = P.GetTranspose();
    tangentialMatrix = tangentialMatrix * P; // P^TP
    normalMatrix = normalMatrix * w; // w^2nn^T

    // Copy the matrices to the diffusion tensor
    for ( unsigned int i = 0; i < ImageDimension; i++ )
      {
      for ( unsigned int j = 0; j < ImageDimension; j++ )
        {
        tangentialDiffusionTensor( i, j ) = tangentialMatrix( i, j );
        normalDiffusionTensor( i, j ) = normalMatrix( i, j );
        }
      }

    // Copy the diffusion tensors to their images
    tangentialTensorIt.Set( tangentialDiffusionTensor );
    normalTensorIt.Set( normalDiffusionTensor );
    }
}

/** Computes the multiplication vectors that the div(Tensor /grad u) values
 *  are multiplied by.
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::ComputeMultiplicationVectorImages()
{
  assert( this->GetComputeRegularizationTerm() );
  assert( this->GetOutput() );
  assert( this->GetNormalVectorImage() );

  // The output will be used as the template to allocate the images we will
  // use to store data computed before/during the registration
  OutputImagePointer output = this->GetOutput();

  // Allocate the images needed when using the anisotropic diffusive
  // regularization
  // There is no multiplication vector for the tangential term, and the
  // superclass will init it to zeros for us.
  // The normal multiplication vector is n_l*n

  // Iterate over the normal vector image
  NormalVectorImageRegionType normalIt = NormalVectorImageRegionType(
      m_NormalVectorImage, m_NormalVectorImage->GetLargestPossibleRegion() );

  NormalVectorType normalVector;
  normalVector.Fill( 0.0 );
  for( int i = 0; i < ImageDimension; i++ )
    {
    // Create the multiplication vector image
    DeformationFieldPointer normalMultsImage = DeformationFieldType::New();
    this->AllocateSpaceForImage( normalMultsImage, output );

    // Calculate n_l*n
    DeformationVectorImageRegionType multIt = DeformationVectorImageRegionType(
        normalMultsImage, normalMultsImage->GetLargestPossibleRegion() );
    for( normalIt.GoToBegin(), multIt.GoToBegin();
         !multIt.IsAtEnd();
         ++normalIt, ++multIt )
           {
      normalVector = normalIt.Get();
      multIt.Set( normalVector[i] * normalVector );
      }

    // Set the multiplication vector image
    this->SetMultiplicationVectorImage( NORMAL, i, normalMultsImage );
    }
}

/**
 * Updates the deformation vector component images before each iteration
 */
template < class TFixedImage, class TMovingImage, class TDeformationField >
void
AnisotropicDiffusiveRegistrationFilter
  < TFixedImage, TMovingImage, TDeformationField >
::UpdateDeformationComponentImages()
{
  assert( this->GetComputeRegularizationTerm() );
  assert( this->GetNormalVectorImage() );
  assert( this->GetWeightImage() );

  // Setup iterators
  NormalVectorImageRegionType normalVectorRegion(
      m_NormalVectorImage, m_NormalVectorImage->GetLargestPossibleRegion() );

  OutputImagePointer output = this->GetOutput();
  OutputImageRegionType outputRegion(output,
                                     output->GetLargestPossibleRegion() );

  DeformationFieldPointer normalDeformationField
      = this->GetDeformationComponentImage( NORMAL );
  OutputImageRegionType normalDeformationRegion(
      normalDeformationField,
      normalDeformationField->GetLargestPossibleRegion() );

  // Calculate the tangential and normal components of the deformation field
  NormalVectorType       n;
  DeformationVectorType  u; // deformation vector
  DeformationVectorType  normalDeformationVector;
  DeformationVectorType  tangentialDeformationVector;

  for( normalVectorRegion.GoToBegin(), outputRegion.GoToBegin(),
       normalDeformationRegion.GoToBegin();
  !outputRegion.IsAtEnd();
  ++normalVectorRegion, ++outputRegion, ++normalDeformationRegion )
    {
    n = normalVectorRegion.Get();
    u = outputRegion.Get();

    // normal component = (u^Tn)n
    normalDeformationVector = ( u * n ) * n;
    normalDeformationRegion.Set( normalDeformationVector );

    // Test that the normal and tangential components were computed corectly
    // (they should be orthogonal)

    // tangential component = u - normal component
    tangentialDeformationVector = u - normalDeformationVector;

    if( normalDeformationVector * tangentialDeformationVector > 0.005 )
      {
      itkExceptionMacro( << "Normal and tangential deformation field "
                         << "components are not orthogonal" );
      }
    }
  normalDeformationField->Modified();
}

} // end namespace itk

#endif
