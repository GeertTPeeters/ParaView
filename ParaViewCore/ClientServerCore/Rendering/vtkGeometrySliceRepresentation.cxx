/*=========================================================================

  Program:   ParaView
  Module:    vtkGeometrySliceRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkGeometrySliceRepresentation.h"

#include "vtkAlgorithmOutput.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkDoubleArray.h"
#include "vtkFieldData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPVCacheKeeper.h"
#include "vtkPVChangeOfBasisHelper.h"
#include "vtkPVGeometryFilter.h"
#include "vtkPVLODActor.h"
#include "vtkPVMultiSliceView.h"
#include "vtkPVOrthographicSliceView.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkStringArray.h"
#include "vtkThreeSliceFilter.h"
#include "vtkVector.h"

#ifndef VTKGL2
# include "vtkCompositePolyDataMapper2.h"
# include "vtkHardwareSelectionPolyDataPainter.h"
#endif
#include <vector>
#include <cassert>
namespace
{
  bool GetNormalsToBasisPlanes(vtkMatrix4x4* changeOfBasisMatrix, vtkVector3d sliceNormals[3])
    {
    if (!changeOfBasisMatrix)
      {
      sliceNormals[0] = vtkVector3d(1, 0, 0);
      sliceNormals[1] = vtkVector3d(0, 1, 0);
      sliceNormals[2] = vtkVector3d(0, 0, 1);
      }
    else
      {
      vtkVector3d axisBases[3];
      vtkPVChangeOfBasisHelper::GetBasisVectors(changeOfBasisMatrix,
        axisBases[0], axisBases[1], axisBases[2]);
      for (int cc=0; cc < 3; cc++)
        {
        sliceNormals[cc] = axisBases[(cc+1)%3].Cross(axisBases[(cc+2)%3]);
        sliceNormals[cc].Normalize();
        }
      }
    return true;
    }

class vtkGSRGeometryFilter : public vtkPVGeometryFilter
{
  std::vector<double> SlicePositions[3];
public:
  /// Set positions for slice locations along each of the basis axis.
  void SetSlicePositions(int axis, const std::vector<double>& positions)
    {
    assert(axis >=0 && axis <= 2);
    if (this->SlicePositions[axis] != positions)
      {
      this->SlicePositions[axis] = positions;
      this->Modified();
      }
    }

  static bool CacheBounds(vtkDataObject* dataObject, const double bounds[6])
    {
    vtkNew<vtkDoubleArray> boundsArray;
    boundsArray->SetName("vtkGSRGeometryFilter_Bounds");
    boundsArray->SetNumberOfComponents(6);
    boundsArray->SetNumberOfTuples(1);
    std::copy(bounds, bounds+6, boundsArray->GetPointer(0));
    dataObject->GetFieldData()->AddArray(boundsArray.GetPointer());
    }
  static bool ExtractCachedBounds(vtkDataObject* dataObject, double bounds[6])
    {
    if (dataObject == NULL || dataObject->GetFieldData() == NULL)
      {
      return false;
      }
    vtkFieldData* fd = dataObject->GetFieldData();
    // first try OrientedBoundingBox if present. These are more accurate when
    // Basis is changed.
    if (vtkPVChangeOfBasisHelper::GetBoundingBoxInBasis(dataObject, bounds))
      {
      return true;
      }
    if (fd->GetArray("vtkGSRGeometryFilter_Bounds") &&
      fd->GetArray("vtkGSRGeometryFilter_Bounds")->GetNumberOfTuples() == 1 &&
      fd->GetArray("vtkGSRGeometryFilter_Bounds")->GetNumberOfComponents() == 6)
      {
      fd->GetArray("vtkGSRGeometryFilter_Bounds")->GetTuple(0, bounds);
      return vtkMath::AreBoundsInitialized(bounds);
      }
    return false;
    }

public:
  static vtkGSRGeometryFilter* New();
  vtkTypeMacro(vtkGSRGeometryFilter, vtkPVGeometryFilter);

  virtual int RequestData(
    vtkInformation *req, vtkInformationVector **inputVector,
    vtkInformationVector *outputVector)
    {
    vtkSmartPointer<vtkDataObject> inputDO = vtkDataObject::GetData(inputVector[0]);
    vtkSmartPointer<vtkMatrix4x4> changeOfBasisMatrix =
      vtkPVChangeOfBasisHelper::GetChangeOfBasisMatrix(inputDO);

    vtkVector3d sliceNormals[3];
    GetNormalsToBasisPlanes(changeOfBasisMatrix, sliceNormals);

    vtkNew<vtkThreeSliceFilter> slicer;
    slicer->SetInputDataObject(inputDO);
    slicer->SetCutOrigins(0, 0, 0);
    for (int axis=0; axis<3; axis++)
      {
      slicer->SetNumberOfSlice(axis, static_cast<int>(this->SlicePositions[axis].size()));
      slicer->SetCutNormal(axis, sliceNormals[axis].GetData());
      for (size_t cc=0; cc < this->SlicePositions[axis].size(); cc++)
        {
        double position[4] = {0, 0, 0, 1};
        position[axis] = this->SlicePositions[axis][cc];
        // The of position specified in the UI is in the coordinate space defined
        // by the changeOfBasisMatrix. We need to convert it to cartesian
        // space.
        if (changeOfBasisMatrix)
          {
          changeOfBasisMatrix->MultiplyPoint(position, position);
          position[0] /= position[3];
          position[1] /= position[3];
          position[2] /= position[3];
          position[3]  = 1.0;
          }
        // project this point on slice normal since we're not directly
        // specifying the slice point but the slice offset along the slice
        // normal from the slice origin (which is (0,0,0)).
        double offset = sliceNormals[axis].Dot(vtkVector3d(position));
        slicer->SetCutValue(axis, static_cast<int>(cc), offset);
        }
      }
    slicer->Update();
    inputVector[0]->GetInformationObject(0)->Set(
      vtkDataObject::DATA_OBJECT(), slicer->GetOutputDataObject(0));
    int ret = this->Superclass::RequestData(req, inputVector, outputVector);
    inputVector[0]->GetInformationObject(0)->Set(
      vtkDataObject::DATA_OBJECT(), inputDO);

    // Add input bounds to the ouput field data so it gets cached for use in
    // vtkGeometrySliceRepresentation::RequestData().
    vtkDataObject* output = vtkDataObject::GetData(outputVector, 0);
    double inputBds[6];
    vtkGeometryRepresentation::GetBounds(inputDO, inputBds);
    vtkGSRGeometryFilter::CacheBounds(output, inputBds);
    return ret;
    }

protected:
  vtkGSRGeometryFilter()
    {
    }

  virtual ~vtkGSRGeometryFilter() {}

private:
  vtkGSRGeometryFilter(const vtkGSRGeometryFilter&);
  void operator=(vtkGSRGeometryFilter&);
};
vtkStandardNewMacro(vtkGSRGeometryFilter);
}


class vtkGeometrySliceRepresentation::vtkInternals
{
public:
  double OriginalDataBounds[6];
  std::pair<bool, std::string> AxisLabels[3];
};

vtkStandardNewMacro(vtkGeometrySliceRepresentation);
//----------------------------------------------------------------------------
vtkGeometrySliceRepresentation::vtkGeometrySliceRepresentation()
  : Internals(new vtkGeometrySliceRepresentation::vtkInternals())
{
  this->GeometryFilter->Delete();
  this->GeometryFilter = vtkGSRGeometryFilter::New();
  this->SetupDefaults();
  this->Mode = ALL_SLICES;
}

//----------------------------------------------------------------------------
vtkGeometrySliceRepresentation::~vtkGeometrySliceRepresentation()
{
  delete this->Internals;
  this->Internals = NULL;
}

//----------------------------------------------------------------------------
void vtkGeometrySliceRepresentation::SetupDefaults()
{
  vtkMath::UninitializeBounds(this->Internals->OriginalDataBounds);
  this->Superclass::SetupDefaults();
#ifndef VTKGL2
  vtkCompositePolyDataMapper2* mapper =
      vtkCompositePolyDataMapper2::SafeDownCast(this->Mapper);
  vtkHardwareSelectionPolyDataPainter* selPainter =
      vtkHardwareSelectionPolyDataPainter::SafeDownCast(
        mapper->GetSelectionPainter()->GetDelegatePainter());
  selPainter->SetPointIdArrayName("-");
  selPainter->SetCellIdArrayName("vtkSliceOriginalCellIds");
  selPainter->SetCompositeIdArrayName("vtkSliceCompositeIndex");
#endif
}

//----------------------------------------------------------------------------
int vtkGeometrySliceRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type,
  vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (this->GetVisibility() == false)
    {
    return 0;
    }

  if (request_type == vtkPVView::REQUEST_UPDATE())
    {
    vtkGSRGeometryFilter* geomFilter = vtkGSRGeometryFilter::SafeDownCast(this->GeometryFilter);
    assert(geomFilter);

    // Propagate slice paramemeters from the view to the representation.
    vtkPVMultiSliceView* view = vtkPVMultiSliceView::SafeDownCast(
      inInfo->Get(vtkPVView::VIEW()));
    if (view)
      {
      for (int mode=X_SLICE_ONLY; mode < ALL_SLICES; mode++)
        {
        if (this->Mode == mode || this->Mode == ALL_SLICES)
          {
          geomFilter->SetSlicePositions(mode, view->GetSlices(mode));
          }
        else
          {
          geomFilter->SetSlicePositions(mode, std::vector<double>());
          }
        }
      if (geomFilter->GetMTime() > this->GetMTime())
        {
        this->MarkModified();
        }
      }
    }
  int retVal = this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo);
  if (retVal && request_type == vtkPVView::REQUEST_UPDATE())
    {
    vtkPVMultiSliceView* view = vtkPVMultiSliceView::SafeDownCast(
      inInfo->Get(vtkPVView::VIEW()));
    if (view)
      {
      vtkPVMultiSliceView::SetDataBounds(inInfo, this->Internals->OriginalDataBounds);
      for (int axis=0; axis<3; axis++)
        {
        if (this->Internals->AxisLabels[axis].first)
          {
          vtkPVMultiSliceView::SetAxisTitle(inInfo, axis,
            this->Internals->AxisLabels[axis].second.c_str());
          }
        }
      }
    if (this->Mode != ALL_SLICES)
      {
      // i.e. being used in vtkPVOrthographicSliceView for showing the
      // orthographic slices. We don't parallel rendering those orthographic
      // views for now.
      vtkPVRenderView::SetDeliverToClientAndRenderingProcesses(
        inInfo, this, true, true);
      }
    }
  if (request_type == vtkPVView::REQUEST_RENDER())
    {
    // This is called on the "rendering" nodes. We use this pass to communicate
    // the "ModelTransformationMatrix" to the view.
    if (vtkPVMultiSliceView* view = vtkPVMultiSliceView::SafeDownCast(
      inInfo->Get(vtkPVView::VIEW())))
      {
      vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
      vtkAlgorithm* algo = producerPort->GetProducer();
      view->SetModelTransformationMatrix(
        vtkPVChangeOfBasisHelper::GetChangeOfBasisMatrix(
          algo->GetOutputDataObject(producerPort->GetIndex())));
      }
    }
  return retVal;
}


//----------------------------------------------------------------------------
int vtkGeometrySliceRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkMath::UninitializeBounds(this->Internals->OriginalDataBounds);
  this->Internals->AxisLabels[0].first = false;
  this->Internals->AxisLabels[1].first = false;
  this->Internals->AxisLabels[2].first = false;
  if (this->Superclass::RequestData(request, inputVector, outputVector))
    {
    // If data-bounds are provided in the meta-data, we will report those to the
    // slice view so that the slice view shows the data bounds to the user when
    // setting up slices.
    vtkDataObject* localData = this->CacheKeeper->GetOutputDataObject(0);
    vtkGSRGeometryFilter::ExtractCachedBounds(
      localData, this->Internals->OriginalDataBounds);
    const char* titles[3] = {NULL, NULL, NULL};
    vtkPVChangeOfBasisHelper::GetBasisName(localData, titles[0], titles[1], titles[2]);
    for (int axis=0; axis < 3; ++axis)
      {
      std::pair<bool, std::string>& label = this->Internals->AxisLabels[axis];
      label.first = (titles[axis] != NULL);
      label.second = titles[axis]? titles[axis] : "";
      }
    return 1;
    }
  return 0;
}

//----------------------------------------------------------------------------
bool vtkGeometrySliceRepresentation::AddToView(vtkView* view)
{
  vtkPVOrthographicSliceView* rview = vtkPVOrthographicSliceView::SafeDownCast(view);
  if (rview && this->Mode != ALL_SLICES)
    {
    rview->GetRenderer(this->Mode)->AddActor(this->Actor);
    return true;
    }
  return this->Superclass::AddToView(view);
}

//----------------------------------------------------------------------------
bool vtkGeometrySliceRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVOrthographicSliceView* rview = vtkPVOrthographicSliceView::SafeDownCast(view);
  if (rview && this->Mode != ALL_SLICES)
    {
    rview->GetRenderer(this->Mode)->RemoveActor(this->Actor);
    return true;
    }
  return this->Superclass::RemoveFromView(view);
}
//----------------------------------------------------------------------------
void vtkGeometrySliceRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
