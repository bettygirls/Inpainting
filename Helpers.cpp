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
namespace Helpers
{

itk::ImageRegion<2> GetRegionInRadiusAroundPixel(itk::Index<2> pixel, unsigned int radius)
{
  // This function returns a Region with the specified 'radius' centered at 'pixel'. By the definition of the radius of a square patch, the output region is (radius*2 + 1)x(radius*2 + 1).

  // The "index" is the lower left corner, so we need to subtract the radius from the center to obtain it
  pixel[0] -= radius;
  pixel[1] -= radius;

  itk::ImageRegion<2> region;
  region.SetIndex(pixel);
  itk::Size<2> size;
  size[0] = radius*2 + 1;
  size[1] = radius*2 + 1;
  region.SetSize(size);

  return region;
}

bool IsValidPatch(const MaskImageType::Pointer mask, itk::ImageRegion<2> region)
{
  itk::ImageRegionConstIterator<MaskImageType> maskIterator(mask,region);

  while(!maskIterator.IsAtEnd())
    {
    if(maskIterator.Get() > 0)
      {
      return false;
      }

    ++maskIterator;
    }
  return true;
}

itk::Index<2> GetRegionCenter(itk::ImageRegion<2> region)
{
  itk::Index<2> center;
  center[0] = region.GetIndex()[0] + region.GetSize()[0] / 2;
  center[1] = region.GetIndex()[1] + region.GetSize()[1] / 2;

  return center;
}

} // end namespace