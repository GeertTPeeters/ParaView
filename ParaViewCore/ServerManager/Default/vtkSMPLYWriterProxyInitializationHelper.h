/*=========================================================================

  Program:   ParaView
  Module:    vtkSMPLYWriterProxyInitializationHelper.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMPLYWriterProxyInitializationHelper - initialization helper for
// (writers, PPLYWriter) proxy.
// .SECTION Description
// vtkSMPLYWriterProxyInitializationHelper is an initialization helper for
// the PPLYWriter proxy that sets up the "ColorArrayName" and "LookupTable"
// using the coloring state in the active view.

#ifndef vtkSMPLYWriterProxyInitializationHelper_h
#define vtkSMPLYWriterProxyInitializationHelper_h

#include "vtkPVServerManagerDefaultModule.h" //needed for exports
#include "vtkSMProxyInitializationHelper.h"

class VTKPVSERVERMANAGERDEFAULT_EXPORT vtkSMPLYWriterProxyInitializationHelper : public vtkSMProxyInitializationHelper
{
public:
  static vtkSMPLYWriterProxyInitializationHelper* New();
  vtkTypeMacro(vtkSMPLYWriterProxyInitializationHelper, vtkSMProxyInitializationHelper);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual void PostInitializeProxy(vtkSMProxy*, vtkPVXMLElement*, unsigned long);

protected:
  vtkSMPLYWriterProxyInitializationHelper();
  ~vtkSMPLYWriterProxyInitializationHelper();

private:
  vtkSMPLYWriterProxyInitializationHelper(const vtkSMPLYWriterProxyInitializationHelper&); // Not implemented
  void operator=(const vtkSMPLYWriterProxyInitializationHelper&); // Not implemented
};

#endif
