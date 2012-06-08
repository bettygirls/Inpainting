/*=========================================================================
 *
 *  Copyright David Doria 2011 daviddoria@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#ifndef CriminisiInpainting_h
#define CriminisiInpainting_h

#include "Types.h"
#include "Helpers.h"

#include "itkAddImageFilter.h"
#include "itkBinaryContourImageFilter.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkBinaryErodeImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkConstNeighborhoodIterator.h"
#include "itkConstantBoundaryCondition.h"
#include "itkCovariantVector.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkFlatStructuringElement.h"
#include "itkGradientImageFilter.h"
#include "itkImage.h"
#include "itkImageDuplicator.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
#include "itkInvertIntensityImageFilter.h"
#include "itkMaskImageFilter.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkPasteImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkRigid2DTransform.h"
#include "itkSubtractImageFilter.h"
#include "itkUnaryFunctorImageFilter.h"
#include "itkVariableLengthVector.h"

#include <iomanip> // setfill, setw

template <class TImage>
class CriminisiInpainting
{
public:

  CriminisiInpainting();

  void Inpaint();
  void SetImage(typename TImage::Pointer image);
  void SetInputMask(MaskImageType::Pointer mask);
  void SetPatchRadius(unsigned int);

  void SetWeights(std::vector<float>);

  // Debugging
  void SetWriteIntermediateImages(bool);

private:
  // Debugging
  bool WriteIntermediateImages;

  // Data members
  typename TImage::Pointer Image;

  UnsignedCharScalarImageType::Pointer InputMask;
  UnsignedCharScalarImageType::Pointer Mask;
  FloatScalarImageType::Pointer ConfidenceImage;
  itk::Size<2> PatchRadius;

  FloatVector2ImageType::Pointer IsophoteImage;
  UnsignedCharScalarImageType::Pointer BoundaryImage;
  FloatVector2ImageType::Pointer BoundaryNormals;

  FloatScalarImageType::Pointer PriorityImage;

  // Functions
  void Initialize();

  // Debugging
  void DebugWriteAllImages(itk::Index<2> pixelToFill, itk::Index<2> bestMatchPixel, unsigned int iteration);
  void DebugWritePatch(itk::Index<2> pixel, std::string filePrefix, unsigned int iteration);
  void DebugWritePatch(itk::ImageRegion<2> region, std::string filename);

  template <typename TDebugImageType>
  void DebugWriteImage(typename TDebugImageType::Pointer image, std::string filePrefix, unsigned int iteration);

  void DebugWritePatch(itk::Index<2> pixel, std::string filename);
  void DebugWritePixelToFill(itk::Index<2> pixelToFill, unsigned int iteration);
  void DebugWritePatchToFillLocation(itk::Index<2> pixelToFill, unsigned int iteration);

  itk::CovariantVector<float, 2> GetAverageIsophote(itk::Index<2> queryPixel);
  bool IsValidPatch(itk::Index<2> queryPixel, unsigned int radius);
  bool IsValidPatch(itk::ImageRegion<2> patch);

  unsigned int GetNumberOfPixelsInPatch();

  itk::Size<2> GetPatchSize();

  void FindBoundary();
  void ComputeIsophotes();
  bool HasMoreToInpaint(MaskImageType::Pointer mask);

  void ComputeBoundaryNormals();

  void ExpandMask();

  // Criminisi specific functions
  void ComputeAllPriorities();
  float ComputePriority(itk::Index<2> queryPixel);
  float ComputeConfidenceTerm(itk::Index<2> queryPixel);
  float ComputeDataTerm(itk::Index<2> queryPixel);

  itk::Index<2> FindHighestPriority(FloatScalarImageType::Pointer priorityImage);

  void UpdateMask(itk::Index<2> pixel);

  // How much weight (0-1) is given to each component of the image
  std::vector<float> Weights;

  void ComputeSourcePatches();
  std::vector<itk::ImageRegion<2> > SourcePatches;
};



#include "CriminisiInpainting.txx"
#include "CriminisiInpaintingDebugging.txx"

#endif