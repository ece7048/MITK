/*=========================================================================
 
Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date$
Version:   $Revision: $
 
Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.
 
This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.
 
=========================================================================*/

#include "mitkDiffSliceOperation.h"

#include <itkCommand.h>

mitk::DiffSliceOperation::DiffSliceOperation():Operation(1)
{
  m_TimeStep = 0;
  m_Slice = NULL;
  m_Image = NULL;
  m_WorldGeometry = NULL;
  m_SliceGeometry = NULL;
  m_ImageIsValid = false;
}


mitk::DiffSliceOperation::DiffSliceOperation(mitk::Image* imageVolume,
                                             vtkImageData* slice,
                                             AffineGeometryFrame3D* sliceGeometry,
                                             unsigned int timestep,
                                             AffineGeometryFrame3D* currentWorldGeometry):Operation(1)

{
  m_WorldGeometry = currentWorldGeometry->Clone();
  m_SliceGeometry = sliceGeometry->Clone();

  m_TimeStep = timestep;

  /*m_zlibSliceContainer = CompressedImageContainer::New();
  m_zlibSliceContainer->SetImage( slice );*/
  m_Slice = vtkSmartPointer<vtkImageData>::New();
  m_Slice->DeepCopy(slice);

  m_Image = imageVolume;

  if ( m_Image) {
    /*add an observer to listen to the delete event of the image, this is necessary because the operation is then invalid*/
    itk::SimpleMemberCommand< DiffSliceOperation >::Pointer command = itk::SimpleMemberCommand< DiffSliceOperation >::New();
    command->SetCallbackFunction( this, &DiffSliceOperation::OnImageDeleted );
    //get the id of the observer, used to remove it later on
    m_DeleteObserverTag = imageVolume->AddObserver( itk::DeleteEvent(), command );


    m_ImageIsValid = true;
  }
  else
    m_ImageIsValid = false;

}

mitk::DiffSliceOperation::~DiffSliceOperation()
{
  
  m_Slice = NULL;
  m_WorldGeometry = NULL;
  //m_zlibSliceContainer = NULL;

  if (m_ImageIsValid)
  {
    //if the image is still there, we have to remove the observer from it
    m_Image->RemoveObserver( m_DeleteObserverTag );
  }
  m_Image = NULL;
}

vtkImageData* mitk::DiffSliceOperation::GetSlice()
{
  //Image::ConstPointer image = m_zlibSliceContainer->GetImage().GetPointer();
  return m_Slice;
}

bool mitk::DiffSliceOperation::IsValid()
{
  return m_ImageIsValid && (m_Slice.GetPointer() != NULL) && (m_WorldGeometry.IsNotNull());//TODO improve
}

void mitk::DiffSliceOperation::OnImageDeleted()
{
  //if our imageVolume is removed e.g. from the datastorage the operation is no lnger valid
  m_ImageIsValid = false;
}