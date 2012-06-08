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

ColorImageType::IndexType CriminisiInpainting::FindBestPatchMatch(itk::Index<2> queryPixel)
{
  // CIELab matching
  if(!this->Image->GetLargestPossibleRegion().IsInside(GetRegionAroundPixel(queryPixel)))
    {
    std::cerr << "queryPixel is too close to the border!" << std::endl;
    exit(-1);
    }

  // Setup a neighborhood iterator over the whole CIELAB image
  itk::ConstNeighborhoodIterator<CIELABImageType, CIELABBoundaryConditionType> movingCIELABPatchIterator(this->PatchRadius, this->CIELABImage, this->CIELABImage->GetLargestPossibleRegion());
  movingCIELABPatchIterator.GoToBegin();

  // Setup a neighborhood iterator over the whole Mask image
  itk::ConstNeighborhoodIterator<UnsignedCharImageType, UnsignedCharBoundaryConditionType> movingMaskPatchIterator(this->PatchRadius, this->Mask, this->Mask->GetLargestPossibleRegion());
  movingMaskPatchIterator.GoToBegin();

  itk::ImageRegionIterator<FloatImageType> meanDifferenceIterator(this->MeanDifferenceImage, this->MeanDifferenceImage->GetLargestPossibleRegion());
  meanDifferenceIterator.GoToBegin();

  // Setup an iterator over the fixed cielab image patch
  itk::ImageRegionConstIterator<CIELABImageType> fixedCIELABPatchIterator(this->CIELABImage, GetRegionAroundPixel(queryPixel));
  fixedCIELABPatchIterator.GoToBegin();

  // Setup an iterator over the fixed mask patch
  itk::ImageRegionConstIterator<UnsignedCharImageType> fixedMaskPatchIterator(this->Mask, GetRegionAroundPixel(queryPixel));
  fixedMaskPatchIterator.GoToBegin();

  unsigned int numberOfPixelsInNeighborhood = this->GetPatchSize()[0] * this->GetPatchSize()[1];
  unsigned int minValidPixels = numberOfPixelsInNeighborhood/4;
  // Loop over every pixel in the image with a neighborhood
  while(!movingCIELABPatchIterator.IsAtEnd())
    {
    double sumDifference = 0;
    fixedCIELABPatchIterator.GoToBegin();
    fixedMaskPatchIterator.GoToBegin();

    // Only consider pixels which have a neighborhood entirely inside of the image
    if(!movingCIELABPatchIterator.InBounds())
      {
      meanDifferenceIterator.Set(-1);
      ++movingCIELABPatchIterator;
      ++movingMaskPatchIterator;
      ++meanDifferenceIterator;
      continue;
      }

    unsigned int valid = 0;
    unsigned int invalid = 0;
    // Loop over all pixels in the neighborhood of the current pixel
    for(unsigned int i = 0; i < numberOfPixelsInNeighborhood; i++)
      {
      // Only compare pixels in the source region
      if(fixedMaskPatchIterator.Get() == 0)
        {
        if(movingMaskPatchIterator.GetPixel(i) != 0)
          {
          invalid++;
          continue;
          }

        valid++;
        CIELABImageType::PixelType patchPixel = fixedCIELABPatchIterator.Get();
        CIELABImageType::PixelType cielabPixel = movingCIELABPatchIterator.GetPixel(i);

        for(unsigned int p = 0; p < 3; p++)
          {
          // CIELAB difference
          sumDifference += std::abs(static_cast<float>(cielabPixel[p]) - static_cast<float>(patchPixel[p]));
          }
        }

      ++fixedCIELABPatchIterator;
      ++fixedMaskPatchIterator;
      }

    if(valid > minValidPixels && invalid == 0)
      {
      meanDifferenceIterator.Set(sumDifference/static_cast<double>(valid));
      }
    else
      {
      meanDifferenceIterator.Set(-1);
      }

    ++meanDifferenceIterator;
    ++movingCIELABPatchIterator;
    ++movingMaskPatchIterator;
    }

  // Paste a blank patch into the correlation image so that we don't match a region within the current patch or anywhere near it
  FloatImageType::Pointer blankPatch = FloatImageType::New();
  CreateConstantFloatBlockingPatch(blankPatch, highestValue);
  CopyPatchIntoImage<FloatImageType>(blankPatch, this->MeanDifferenceImage, queryPixel);

  // Blank the masked region in the difference image
  //meanDifferenceIterator.GoToBegin(); // Don't know why we can't reuse this iterator - Arithmetic exception is thrown from itkImageHelper
  itk::ImageRegionIterator<FloatImageType> correlationIterator(this->MeanDifferenceImage,this->MeanDifferenceImage->GetLargestPossibleRegion());
  correlationIterator.GoToBegin();

  itk::ImageRegionConstIterator<UnsignedCharImageType> maskIterator(this->Mask,this->Mask->GetLargestPossibleRegion());
  maskIterator.GoToBegin();

  while(!maskIterator.IsAtEnd())
    {
    if(maskIterator.Get() != 0)
      {
      correlationIterator.Set(highestValue);
      }

    ++maskIterator;
    ++correlationIterator;
    }

  // Replace -1's with the highest value (we do NOT want to choose these points)
  ReplaceValue(this->MeanDifferenceImage, -1, highestValue);
  WriteImage<FloatImageType>(this->MeanDifferenceImage, "BlankedCorrelation.mhd");

  imageCalculatorFilter->SetImage(this->MeanDifferenceImage);
  imageCalculatorFilter->Compute();

  UnsignedCharImageType::IndexType bestMatchIndex = imageCalculatorFilter->GetIndexOfMinimum();
  std::cout << "BestMatchIndex: " << bestMatchIndex << std::endl;
  return bestMatchIndex;
}



void CriminisiInpainting::UpdateIsophoteImage(FloatImageType::IndexType sourcePixel, itk::Index<2> targetPixel)
{
  // Copy isophotes from best patch
  typedef itk::RegionOfInterestImageFilter< VectorImageType,
                                            VectorImageType > ExtractFilterType;

  ExtractFilterType::Pointer extractFilter = ExtractFilterType::New();
  extractFilter->SetRegionOfInterest(GetRegionInRadiusAroundPixel(sourcePixel, this->PatchRadius[0]));
  extractFilter->SetInput(this->IsophoteImage);
  extractFilter->Update();

  VectorImageType::Pointer isophoteImagePatch = extractFilter->GetOutput();

  itk::ImageRegionIterator<VectorImageType> imageIterator(isophoteImagePatch, isophoteImagePatch->GetLargestPossibleRegion());
  itk::ImageRegionIterator<UnsignedCharImageType> maskIterator(this->Mask, GetRegionInRadiusAroundPixel(sourcePixel, this->PatchRadius[0]));
  imageIterator.GoToBegin();
  maskIterator.GoToBegin();

  // "clear" the pixels which are in the target region.
  VectorImageType::PixelType blankPixel;
  blankPixel.Fill(0);

  while(!imageIterator.IsAtEnd())
  {
    if(maskIterator.Get() != 0) // we are in the target region
      {
      imageIterator.Set(blankPixel);
      }

    ++imageIterator;
    ++maskIterator;
  }

  CopyPatchIntoImage<VectorImageType>(isophoteImagePatch, this->IsophoteImage, this->Mask, targetPixel);
}

void CriminisiInpainting::UpdateConfidenceImage(FloatImageType::IndexType sourcePixel, FloatImageType::IndexType targetPixel)
{
  // Loop over all pixels in the target region which were just filled and update their confidence.
  itk::ImageRegionIterator<FloatImageType> confidenceIterator(this->ConfidenceImage, GetRegionInRadiusAroundPixel(targetPixel, this->PatchRadius[0]));
  itk::ImageRegionIterator<UnsignedCharImageType> maskIterator(this->Mask, GetRegionInRadiusAroundPixel(targetPixel, this->PatchRadius[0]));
  confidenceIterator.GoToBegin();
  maskIterator.GoToBegin();

  while(!maskIterator.IsAtEnd())
    {
    if(maskIterator.Get()) // This was a target pixel, compute its confidence
      {
      confidenceIterator.Set(ComputeConfidenceTerm(maskIterator.GetIndex()));
      }
    ++confidenceIterator;
    ++maskIterator;
    }
}

void something()
{
  // Extract the best patch
  typedef itk::RegionOfInterestImageFilter< ColorImageType,
                                            ColorImageType > ExtractFilterType;

  ExtractFilterType::Pointer extractFilter = ExtractFilterType::New();
  extractFilter->SetRegionOfInterest(GetRegionInRadiusAroundPixel(bestMatchPixel, this->PatchRadius[0]));
  extractFilter->SetInput(this->Image);
  extractFilter->Update();
  this->Patch->Graft(extractFilter->GetOutput());

  CopyPatchIntoImage<ColorImageType>(this->Patch, this->Image, this->Mask, pixelToFill);

}


template <class T>
void CopySelfPatchIntoValidRegion(typename T::Pointer image, UnsignedCharImageType::Pointer mask,
                                  itk::Index<2> sourcePixel, itk::Index<2> destinationPixel, unsigned int radius);

template <class T>
void CopySelfPatchIntoValidRegion(typename T::Pointer image, UnsignedCharImageType::Pointer mask, itk::Index<2> sourcePixel, itk::Index<2> destinationPixel, unsigned int patchRadius)
{
  try
  {
  // This function copies a patch of radius 'radius' into itself only where the 'mask' is non-zero.
  // 'mask' must be the same size as 'image'.

  itk::Size<2> radius;
  radius.Fill(patchRadius);

  itk::Size<2> onePixelSize;
  onePixelSize.Fill(1);

  itk::ImageRegion<2> sourceRegion(sourcePixel, onePixelSize); // This is a 1 pixel region indicating the center of the patch. The neighborhood iterator takes care of creating the actual region

  itk::ImageRegion<2> destinationRegion(destinationPixel, onePixelSize); // This is a 1 pixel region indicating the center of the patch. The neighborhood iterator takes care of creating the actual region

  itk::ConstNeighborhoodIterator<T> sourcePatchIterator(radius, image, sourceRegion);
  itk::NeighborhoodIterator<T> destinationPatchIterator(radius, image, destinationRegion);

  itk::ConstNeighborhoodIterator<UnsignedCharImageType> destinationMaskPatchIterator(radius, mask, destinationRegion);

  typename T::PixelType sourcePixelValue;
  typename T::PixelType destinationPixelValue;
  typename UnsignedCharImageType::PixelType maskPixelValue;

  while(!destinationPatchIterator.IsAtEnd())
    {
    for(unsigned int i = 0; i < (patchRadius*2 + 1)*(patchRadius*2 + 1); i++) // the number of pixels in the neighborhood
      {
      bool sourceIsInBounds;
      bool destinationIsInBounds;
      sourcePixelValue = sourcePatchIterator.GetPixel(i, sourceIsInBounds);
      destinationPixelValue = destinationPatchIterator.GetPixel(i, destinationIsInBounds);
      if(!(sourceIsInBounds && destinationIsInBounds))
        {
        // do nothing
        }
      else
        {
        bool inbounds; // this is not used
        maskPixelValue = destinationMaskPatchIterator.GetPixel(i, inbounds);

        if(maskPixelValue) // we are in the target region
          {
          destinationPatchIterator.SetPixel(i, sourcePixelValue);
          }
        }// end else
      } // end for loop over neighborhood pixels
      ++sourcePatchIterator;
      ++destinationMaskPatchIterator;
      ++destinationPatchIterator;
    } // end while loop (should only be one iteration)

  } // end try
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in CopySelfPatchIntoValidRegion!" << std::endl;
    std::cerr << err << std::endl;
    std::cout << "sourcePixel: " << sourcePixel
              << "destinationPixel: " << destinationPixel
              << "patchRadius: " << patchRadius << std::endl;
    exit(-1);
  }
}


// This version requires both patches to be entirely within the image region
template <class T>
void CopySelfPatchIntoValidRegion(typename T::Pointer image, UnsignedCharImageType::Pointer mask, itk::Index<2> sourcePixel, itk::Index<2> destinationPixel, unsigned int radius)
{
  try
  {
  // This function copies a patch of radius 'radius' into itself only where the 'mask' is non-zero.
  // 'mask' must be the same size as 'image'.

  itk::ImageRegion<2> sourceRegion = GetRegionInRadiusAroundPixel(sourcePixel, radius);
  itk::ImageRegion<2> destinationRegion = GetRegionInRadiusAroundPixel(destinationPixel, radius);

  itk::ImageRegionConstIterator<T> sourceIterator(image, sourceRegion);
  itk::ImageRegionConstIterator<UnsignedCharImageType> maskIterator(mask,destinationRegion);
  itk::ImageRegionIterator<T> destinationIterator(image, destinationRegion);

  while(!destinationIterator.IsAtEnd())
    {
    if(maskIterator.Get()) // we are in the target region
      {
      destinationIterator.Set(sourceIterator.Get());
      }
    ++sourceIterator;
    ++maskIterator;
    ++destinationIterator;
    }

  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in CopySelfPatchIntoValidRegion!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}


// This is the main driver function. It calls PatchImageDifference and then finds the best match in the area we want to match from and returns it.
// This was written for an inpainting algorithm - so the 'mask' should be interpreted as the region to inpaint
// (i.e. no data was defined in the 'image' in the 'mask != 0' region.)
// We want the best matching patch to come from a region that has no 'missing' pixels - i.e. it does not overlap the non-zero region of the mask.
template <typename TImage, typename TMask>
itk::Index<2> SelfPatchMatch(TImage* image, TMask* mask, itk::Index<2> queryPixel, unsigned int patchRadius, unsigned int iteration, std::vector<float> weights)
{
  try
  {
  FloatImageType::Pointer differenceImage = FloatImageType::New();

  ComputeDifferenceImage(image, mask, queryPixel, patchRadius, differenceImage, weights);

  // Compute the maximum value of the difference image. We will use this to fill the constant blank patch
  //float highestValue = MaxValue<FloatImageType>(differenceImage);
  float highestValue = itk::NumericTraits< typename FloatImageType::PixelType >::max();

  /*
  // ** Don't need this because we require the source patches to be complete - so if they are near the target region they will never get tried.
  // Paste a blank patch into the correlation image so that we don't match a region within the current patch or anywhere near it
  FloatImageType::Pointer blankPatch = FloatImageType::New();
  Helpers::CreateConstantPatch<FloatImageType>(blankPatch, highestValue, patchRadius * 2);
  Helpers::CopyPatchIntoImage<FloatImageType>(blankPatch, differenceImage, queryPixel);
  */

  // Set the differenceImage pixels in the masked region to a very high value (we don't want to match here)
  itk::ImageRegionIterator<FloatImageType> differenceImageIterator(differenceImage, differenceImage->GetLargestPossibleRegion());
  differenceImageIterator.GoToBegin();

  itk::ImageRegionConstIterator<TMask> maskIterator(mask, mask->GetLargestPossibleRegion());
  maskIterator.GoToBegin();

  while(!differenceImageIterator.IsAtEnd())
    {
    if(maskIterator.Get() > 0)
      {
      differenceImageIterator.Set(highestValue);
      }
    ++differenceImageIterator;
    ++maskIterator;
    }

  std::stringstream padded;
  padded << "Difference_" << std::setfill('0') << std::setw(4) << iteration << ".png";
  Helpers::WriteScaledScalarImage<FloatImageType>(differenceImage, padded.str());

  itk::Index<2> bestMatchIndex = Helpers::MinValueLocation<FloatImageType>(differenceImage);
  //std::cout << "BestMatchIndex: " << bestMatchIndex << std::endl;
  return bestMatchIndex;

  }
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in SelfPatchMatch!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

template <typename TImage, typename TMask>
itk::Index<2> SelfPatchMatch(TImage* image, TMask* mask, itk::Index<2> queryPixel, unsigned int patchRadius)
{
  unsigned int iteration = 0;
  std::vector<float> weights(TImage::PixelType::Dimension, 1.0/static_cast<float>(TImage::PixelType::Dimension));
  return SelfPatchMatch(image, mask, queryPixel, patchRadius, iteration, weights);
}


template <typename TImage, typename TMask>
itk::Index<2> SelfPatchMatch(TImage* image, TMask* mask, itk::Index<2> queryPixel, unsigned int patchRadius, unsigned int iteration, std::vector<float> weights);

template <typename TImage, typename TMask>
itk::Index<2> SelfPatchMatch(TImage* image, TMask* mask, itk::Index<2> queryPixel, unsigned int patchRadius);


template< typename TImage, typename TMask>
void ComputeDifferenceImage(const TImage* image, const TMask* mask,
                            itk::Index<2> queryPixel, unsigned int patchRadius, FloatImageType* output, std::vector<float> weights);



template< typename TImage, typename TMask>
void ComputeDifferenceImage(const TImage* image, const TMask* mask,
                            itk::Index<2> queryPixel, unsigned int patchRadius, FloatImageType* output, std::vector<float> weights)
{
  try
  {
  // This function moves a patch from 'image' centered at 'queryPixel' over the entire 'image' and computes the mean difference of the valid pixels at each location in the sweep.
  // Only patch with zero invalid (masked) pixels are compared.

  // Create an output image the same size as the input image
  output->SetRegions(image->GetLargestPossibleRegion());
  output->Allocate();

  typedef itk::ImageRegionConstIterator<TImage> ConstIteratorType;
  typedef itk::ImageRegionIterator<TImage> IteratorType;
  typedef itk::ImageRegionIterator<FloatImageType> OutputIteratorType;

  typedef itk::NeighborhoodAlgorithm
    ::ImageBoundaryFacesCalculator< TImage> FaceCalculatorType;

  FaceCalculatorType faceCalculator;

  typename FaceCalculatorType::FaceListType faceList;
  itk::Size<2> radiusSize;
  radiusSize.Fill(patchRadius);
  faceList = faceCalculator(image, image->GetLargestPossibleRegion(), radiusSize);

  typename FaceCalculatorType::FaceListType::iterator faceListIterator = faceList.begin();

  itk::ImageRegionConstIteratorWithIndex<TImage> centralIterator(image,*faceListIterator);
  OutputIteratorType outputIterator(output,*faceListIterator);
  centralIterator.GoToBegin();
  outputIterator.GoToBegin();

  while(!centralIterator.IsAtEnd())
    {
    float difference = PatchDifference(image, mask, queryPixel, centralIterator.GetIndex(), patchRadius, weights);
    outputIterator.Set(difference);

    ++centralIterator;
    ++outputIterator;
    }

  ++faceListIterator;
  // Iterate over all of the boundary regions setting their distance to a very high value

  while(faceListIterator != faceList.end())
    {
    OutputIteratorType outputFaceIterator(output,*faceListIterator);
    outputFaceIterator.GoToBegin();
    while(!outputFaceIterator.IsAtEnd())
      {
      outputFaceIterator.Set(itk::NumericTraits< typename FloatImageType::PixelType >::max());
      ++outputFaceIterator;
      }
    ++faceListIterator;
    }

  Helpers::WriteScaledScalarImage<FloatImageType>(output, "DifferenceImage.png");

  }// end try
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in ComputeDifferenceImage!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}
