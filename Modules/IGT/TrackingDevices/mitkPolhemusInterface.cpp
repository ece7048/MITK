/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include <mitkPolhemusInterface.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <PDI.h>

BYTE  MotionBuf[0x1FA400];

mitk::PolhemusInterface::PolhemusInterface() : m_continousTracking(false)
{
  m_pdiDev = new CPDIdev();
  m_numberOfTools = 0;
}

mitk::PolhemusInterface::~PolhemusInterface()
{
  delete m_pdiDev;
}

bool mitk::PolhemusInterface::InitializeDevice()
{
  m_pdiDev->ResetTracker();
  m_pdiDev->ResetSAlignment(-1);
  m_pdiDev->Trace(TRUE, 7);
  m_continousTracking = false;
  return true;
}

bool mitk::PolhemusInterface::SetupDevice()
{
  m_pdiDev->SetPnoBuffer(MotionBuf, 0x1FA400);
  m_pdiDev->SetMetric(true); //use cm instead of inches

  m_pdiDev->StartPipeExport();

  CPDImdat pdiMDat;
  pdiMDat.Empty();
  pdiMDat.Append(PDI_MODATA_FRAMECOUNT);
  pdiMDat.Append(PDI_MODATA_POS);
  pdiMDat.Append(PDI_MODATA_ORI);
  m_pdiDev->SetSDataList(-1, pdiMDat);

  CPDIbiterr cBE;
  m_pdiDev->GetBITErrs(cBE);

  if (!(cBE.IsClear())) { m_pdiDev->ClearBITErrs(); }

  return true;
}

bool mitk::PolhemusInterface::StartTracking()
{
  m_continousTracking = true;
  return m_pdiDev->StartContPno(0);
}

bool mitk::PolhemusInterface::StopTracking()
{
  m_continousTracking = false;
  m_pdiDev->StopContPno();
  return true;
}

bool mitk::PolhemusInterface::Connect()
{
  bool returnValue;
  //Initialize, and if it is not successful, return false.
  if (!InitializeDevice())
  {
    returnValue = false;
  }
  //Connect
  else if (m_pdiDev->CnxReady())
  {
    returnValue = true;
  }
  //If it is not successful, search for connections.
  else
  {
    CPDIser  pdiSer;
    m_pdiDev->SetSerialIF(&pdiSer);

    ePiCommType eType = m_pdiDev->DiscoverCnx();
    switch (eType)
    {
    case PI_CNX_USB:
      MITK_INFO << "USB Connection: " << m_pdiDev->GetLastResultStr();
      break;
    case PI_CNX_SERIAL:
      MITK_INFO << "Serial Connection: " << m_pdiDev->GetLastResultStr();
      break;
    default:
      MITK_INFO << "DiscoverCnx result: " << m_pdiDev->GetLastResultStr();
      break;
    }

    //Setup device
    if (!SetupDevice())
    {
      returnValue = false;
    }
    else
    {
      returnValue = m_pdiDev->CnxReady();
    }
  }

  if (returnValue)
  {
    m_numberOfTools = this->GetNumberOfTools();
  }

  //Get the tracking data to find out which tools are available.
  std::vector<mitk::PolhemusInterface::trackingData> _trackingData;
  if (m_continousTracking)
  {
    _trackingData = GetLastFrame();
  }
  else
  {
    _trackingData = GetSingleFrame();
  }

  //if we had tool before, check if they are still the same.
  if (m_ToolPorts.size() == _trackingData.size())
  {
    for (int i = 0; i < _trackingData.size(); ++i)
    {
      //if they are not the same, clear hemispheres and toolNames and break.
      if (m_ToolPorts[i] != _trackingData.at(i).id)
      {
        m_ToolPorts.clear();
        m_Hemispheres.clear();
        break;
      }
    }
  }

  //if we don't have old tool names or if the old ones don't match any more, assign them again.
  if (m_ToolPorts.size() == 0)
  {
    for (int i = 0; i < _trackingData.size(); ++i)
    {
      m_ToolPorts.push_back(_trackingData.at(i).id);
    }
  }

  return returnValue;
}

bool mitk::PolhemusInterface::Disconnect()
{
  bool returnValue = true;
  //If Tracking is running, stop tracking first
  if (m_continousTracking)
  {
    this->StopTracking();
  }

  returnValue = m_pdiDev->Disconnect();
  MITK_INFO << "Disconnect: " << m_pdiDev->GetLastResultStr();
  return returnValue;
}

std::vector<mitk::PolhemusInterface::trackingData> mitk::PolhemusInterface::GetLastFrame()
{
  PBYTE pBuf;
  DWORD dwSize;

  //read one frame
  if (!m_pdiDev->LastPnoPtr(pBuf, dwSize)) { MITK_WARN << m_pdiDev->GetLastResultStr(); }

  std::vector<mitk::PolhemusInterface::trackingData> returnValue = ParsePolhemusRawData(pBuf, dwSize);

  if (returnValue.empty())
  {
    MITK_WARN << "Cannot parse data / no tools present";
  }

  return returnValue;
}

unsigned int mitk::PolhemusInterface::GetNumberOfTools()
{
  if (m_continousTracking) return GetLastFrame().size();
  else return GetSingleFrame().size();
}

std::vector<mitk::PolhemusInterface::trackingData> mitk::PolhemusInterface::GetSingleFrame()
{
  if (m_continousTracking)
  {
    MITK_WARN << "Cannot get a single frame when continuous tracking is on!";
    return std::vector<mitk::PolhemusInterface::trackingData>();
  }
  PBYTE pBuf;
  DWORD dwSize;

  //read one frame
  if (!m_pdiDev->ReadSinglePnoBuf(pBuf, dwSize)) {
    MITK_WARN << m_pdiDev->GetLastResultStr();
    return std::vector<mitk::PolhemusInterface::trackingData>();
  }

  return ParsePolhemusRawData(pBuf, dwSize);
}

std::vector<mitk::PolhemusInterface::trackingData> mitk::PolhemusInterface::ParsePolhemusRawData(PBYTE pBuf, DWORD dwSize)
{
  std::vector<mitk::PolhemusInterface::trackingData> returnValue;

  DWORD i = 0;

  while (i < dwSize)
  {
    BYTE ucSensor = pBuf[i + 2];
    SHORT shSize = pBuf[i + 6];

    // skip rest of header
    i += 8;

    PDWORD pFC = (PDWORD)(&pBuf[i]);
    PFLOAT pPno = (PFLOAT)(&pBuf[i + 4]);

    mitk::PolhemusInterface::trackingData currentTrackingData;

    currentTrackingData.id = ucSensor;

    currentTrackingData.pos[0] = pPno[0] * 10; //from cm to mm
    currentTrackingData.pos[1] = pPno[1] * 10;
    currentTrackingData.pos[2] = pPno[2] * 10;

    double azimuthAngle = pPno[3] / 180 * M_PI; //from degree to rad
    double elevationAngle = pPno[4] / 180 * M_PI;
    double rollAngle = pPno[5] / 180 * M_PI;
    vnl_quaternion<double> eulerQuat(rollAngle, elevationAngle, azimuthAngle);
    currentTrackingData.rot = eulerQuat;

    returnValue.push_back(currentTrackingData);
    i += shSize;
  }
  return returnValue;
}

void mitk::PolhemusInterface::SetHemisphereTrackingEnabled(bool _HeisphereTrackingEnabeled)
{
  //only if connection is ready!
  if (!this->m_pdiDev->CnxReady())
    return;

  //HemisphereTracking is switched on by SetSHemiTrack(-1). "-1" means for all sensors.
  //To switch heisphere tracking of, you need to set a hemisphere vector by calling SetSHemisphere(-1, { (float)1,0,0 })
  if (_HeisphereTrackingEnabeled)
  {
    //Remember the Hemisphere and Position when switching on to avoid wrong positions ("jumps") when swithing HemiTracking off.
    if (m_Hemispheres.empty())
    {
      //only if it is empty. Otherwise, it might already be on and we overwrite it with (0|0|0).
      for (int i = 0; i < m_numberOfTools; ++i)
      {
        m_Hemispheres.push_back(GetHemisphere(m_ToolPorts[i]));
      }
    }
    m_pdiDev->SetSHemiTrack(-1);
  }

  //switch HemiTracking OFF
  else
  {
    //Get Tool Position. ToDo, this should not be the tool tip but the sensor position. Any chance, to get that from Polhemus interface?!
    std::vector<mitk::PolhemusInterface::trackingData> _position;
    if (m_continousTracking)
    {
      _position = GetLastFrame();
    }
    else
    {
      _position = GetSingleFrame();
    }

    if (m_Hemispheres.empty())
    {
      //Default Hemisphere for all tools, maybe the first setup.
      //But we still check the position, 'cause maybe the tool is in negative space.
      //We can't do that every time, in case the user wants to use e.g. (0|1|0). Therefore, storing the last one makes sense...
      mitk::Vector3D temp;
      mitk::FillVector3D(temp, 1, 0, 0);
      m_Hemispheres.assign(m_numberOfTools, temp);
    }

    for (int i = 0; i < m_numberOfTools; ++i)
    {
      if (m_Hemispheres.at(i).GetNorm() == 0)
      {
        //hemisphere vector can be 0 if Polhemus was in "HemiTracking" status when MITK connected or when user manually set it...
        mitk::FillVector3D(m_Hemispheres.at(i), 1, 0, 0);
      }

      //Scalar product between mitk::point and mitk::vector
      double _scalarProduct = _position.at(i).pos.GetVectorFromOrigin() * m_Hemispheres.at(i);
      //if scalar product is negative, then the tool is in the opposite sphere then when we started to track.
      //Hence, we have to set the inverted hemisphere.
      //For default (1|0|0) this means, if x is negative, we have to set (-1|0|0). But we want to keep it generic if user sets different hemisphere...
      if (_scalarProduct < 0)
      {
        m_Hemispheres.at(i) = -1. * m_Hemispheres.at(i);
      }

      SetHemisphere(m_ToolPorts[i], m_Hemispheres.at(i));
    }
    //clean up hemispheres!
    m_Hemispheres.clear();
  }
}

void mitk::PolhemusInterface::ToggleHemisphere(int _tool)
{
  //only if connection is ready!
  if (!this->m_pdiDev->CnxReady())
    return;

  //we have a single tool number, which is identical with Polhemus index, i.e. first tool is "1", not "0"...
  //is hemiTracking on?
  //Get function again doesn't work in continuous mode...
  BOOL _hemiTrack;
  if (m_continousTracking)
  {
    m_pdiDev->StopContPno();
  }
  m_pdiDev->GetSHemiTrack(_tool, _hemiTrack);
  if (m_continousTracking)
  {
    m_pdiDev->StartContPno(0);
  }

  MITK_INFO << "HemisphereTracking: " << m_pdiDev->GetLastResultStr();

  //if hemiTracing is on, switch it off.
  if (_hemiTrack)
    SetHemisphereTrackingEnabled(false);

  //toggel.
  if (_tool == -1)
  {
    //GetHemisphere(-1) returns the first tool. Hence, we have to loop over all tools manually...
    for (int i = 0; i < m_numberOfTools; ++i)
    {
      this->SetHemisphere(m_ToolPorts[i], -1.*this->GetHemisphere(m_ToolPorts[i]));
    }
  }
  else
  {
    this->SetHemisphere(_tool, -1.*this->GetHemisphere(_tool));
  }

  //if hemiTracing was on, switch it on again.
  if (_hemiTrack)
    SetHemisphereTrackingEnabled(true);
}

void mitk::PolhemusInterface::SetHemisphere(int _tool, mitk::Vector3D _hemisphere)
{
  //only if connection is ready!
  if (!this->m_pdiDev->CnxReady())
    return;

  m_pdiDev->SetSHemisphere(_tool, { (float)_hemisphere[0], (float)_hemisphere[1], (float)_hemisphere[2] });
}

mitk::Vector3D mitk::PolhemusInterface::GetHemisphere(int _tool)
{
  //only if connection is ready!
  if (!this->m_pdiDev->CnxReady())
    return nullptr;

  PDI3vec _hemisphere;
  mitk::Vector3D _returnVector;

  //Doesn't work in continuous mode. Don't know why, but so it is... Hence: stop and restart...
  if (m_continousTracking)
  {
    m_continousTracking = false;
    m_pdiDev->StopContPno();

    m_pdiDev->GetSHemisphere(_tool, _hemisphere);
    MITK_DEBUG << "Get Hemisphere: " << m_pdiDev->GetLastResultStr();
    mitk::FillVector3D(_returnVector, _hemisphere[0], _hemisphere[1], _hemisphere[2]);

    m_pdiDev->StartContPno(0);
    m_continousTracking = true;
  }
  else
  {
    m_pdiDev->GetSHemisphere(_tool, _hemisphere);
    MITK_DEBUG << "Get Hemisphere: " << m_pdiDev->GetLastResultStr();
    mitk::FillVector3D(_returnVector, _hemisphere[0], _hemisphere[1], _hemisphere[2]);
  }

  return _returnVector;
}

void mitk::PolhemusInterface::PrintStatus()
{
  MITK_INFO << "Polhemus status: " << this->m_pdiDev->CnxReady();
}

std::vector<int> mitk::PolhemusInterface::GetToolPorts()
{
  return m_ToolPorts;
}