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

#include "Helpers.h"
#include "Types.h"

#include "itkNeighborhoodAlgorithm.h"
#include "itkAndImageFilter.h"
#include "itkNumericTraits.h"

#include <iomanip> // setfill, setw

template< typename TImage, typename TMask>
float PatchDifference(const TImage* image, const TMask* mask, itk::ImageRegion<2> sourceRegion, itk::ImageRegion<2> targetRegion)
{
  try
  {
  assert(image->GetLargestPossibleRegion().IsInside(sourceRegion));

  if(!image->GetLargestPossibleRegion().IsInside(targetRegion))
    {
    // Move the source region to the target region
    itk::Offset<2> offset = targetRegion.GetIndex() - sourceRegion.GetIndex();
    sourceRegion.SetIndex(sourceRegion.GetIndex() + offset);

    // Make the target region be entirely inside the image
    targetRegion.Crop(image->GetLargestPossibleRegion());
    sourceRegion.Crop(image->GetLargestPossibleRegion());

    // Move the source region back
    sourceRegion.SetIndex(sourceRegion.GetIndex() - offset);
    }

  itk::ImageRegionConstIteratorWithIndex<TImage> sourcePatchIterator(image, sourceRegion);
  itk::ImageRegionConstIteratorWithIndex<TImage> targetPatchIterator(image, targetRegion);
  itk::ImageRegionConstIteratorWithIndex<TMask> maskIterator(mask, targetRegion);

  double sum = 0;
  unsigned int validPixelCounter = 0;
  while(!sourcePatchIterator.IsAtEnd())
    {
    if(!maskIterator.Get()) // we are at an unmasked (i.e. valid) pixel
      {
      double difference = Helpers::PixelSquaredDifference(sourcePatchIterator.Get(), targetPatchIterator.Get());
      sum +=  difference;
      validPixelCounter++;
      }

    ++sourcePatchIterator;
    ++targetPatchIterator;
    ++maskIterator;
    } // end while

  if(validPixelCounter == 0)
    {
    std::cout << "Warning: zero valid pixels in PatchDifference." << std::endl;
    return 0;
    }
  return sum/static_cast<float>(validPixelCounter);
  } //end try
  catch( itk::ExceptionObject & err )
  {
    std::cerr << "ExceptionObject caught in PatchDifference!" << std::endl;
    std::cerr << err << std::endl;
    exit(-1);
  }
}

// This function computes the average difference of the corresponding pixels in a region around 'queryPixel' and 'currentPixel' where 'mask' is non-zero. We require all pixels in region 2 to be valid, so that we guarantee when the patch is copied the region will be filled in with valid pixels.
template< typename TImage, typename TMask>
float PatchDifference(const TImage* image, const TMask* mask, itk::Index<2> queryPixel, itk::Index<2> currentPixel, unsigned int patchRadius, std::vector<float> weights)
{
  itk::ImageRegion<2> queryRegion = Helpers::GetRegionInRadiusAroundPixel(queryPixel, patchRadius);
  itk::ImageRegion<2> currentRegion = Helpers::GetRegionInRadiusAroundPixel(currentPixel, patchRadius);
  return PatchDifference(image, mask, queryRegion, currentRegion);
}

template< typename TImage, typename TMask>
float PatchDifference(const TImage* image, const TMask* mask, itk::Index<2> queryPixel, itk::Index<2> currentPixel, unsigned int patchRadius)
{
  std::vector<float> weights(TImage::PixelType::Dimension, 1.0/static_cast<float>(TImage::PixelType::Dimension));
  return PatchDifference(image, mask, queryPixel, currentPixel, patchRadius, weights);
}


template< typename TImage, typename TMask>
unsigned int BestPatch(const TImage* image, const TMask* mask, std::vector<itk::ImageRegion<2> > sourceRegions, itk::ImageRegion<2> targetRegion)
{
  float minDistance = std::numeric_limits<float>::infinity();
  unsigned int bestMatchId = 0;
  for(unsigned int i = 0; i < sourceRegions.size(); i++)
    {
    float distance = PatchDifference(image, mask, sourceRegions[i], targetRegion);
    //std::cout << "Patch " << i << " distance " << distance << std::endl;
    if(distance < minDistance)
      {
      minDistance = distance;
      bestMatchId = i;
      }
    }

  return bestMatchId;
}
