#include "VisibilityState.h"

using namespace tuvok;

VisibilityState::VisibilityState() : m_eRenderMode(AbstrRenderer::RM_INVALID)
{}

bool VisibilityState::NeedsUpdate(double fMin, double fMax)
{
  bool const bNeedsUpdate = (m_eRenderMode != AbstrRenderer::RM_1DTRANS) ||
    (m_rm1DTrans.fMin != fMin) ||
    (m_rm1DTrans.fMax != fMax);
  m_eRenderMode = AbstrRenderer::RM_1DTRANS;
  m_rm1DTrans.fMin = fMin;
  m_rm1DTrans.fMax = fMax;
  return bNeedsUpdate;
}

bool VisibilityState::NeedsUpdate(double fMin, double fMax, double fMinGradient, double fMaxGradient)
{
  bool const bNeedsUpdate = (m_eRenderMode != AbstrRenderer::RM_2DTRANS) ||
    (m_rm2DTrans.fMin != fMin) ||
    (m_rm2DTrans.fMax != fMax) ||
    (m_rm2DTrans.fMinGradient != fMinGradient) ||
    (m_rm2DTrans.fMaxGradient != fMaxGradient);
  m_eRenderMode = AbstrRenderer::RM_2DTRANS;
  m_rm2DTrans.fMin = fMin;
  m_rm2DTrans.fMax = fMax;
  m_rm2DTrans.fMinGradient = fMinGradient;
  m_rm2DTrans.fMaxGradient = fMaxGradient;
  return bNeedsUpdate;
}

bool VisibilityState::NeedsUpdate(double fIsoValue)
{
  bool const bNeedsUpdate = (m_eRenderMode != AbstrRenderer::RM_ISOSURFACE) ||
    (m_rmIsoSurf.fIsoValue != fIsoValue);
  m_eRenderMode = AbstrRenderer::RM_ISOSURFACE;
  m_rmIsoSurf.fIsoValue = fIsoValue;
  return bNeedsUpdate;
}
