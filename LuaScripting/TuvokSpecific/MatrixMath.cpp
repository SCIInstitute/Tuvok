#include <sstream>
#include <string>
#include "Basics/Vectors.h"
#include "../LuaScripting.h"
#include "LuaTuvokTypes.h"
#include "MatrixMath.h"

static FLOATMATRIX4 RotateX(float angle) {
  FLOATMATRIX4 matRot;
  matRot.RotationX(3.141592653589793238462643383*angle/180.0);
  return matRot;
}
static FLOATMATRIX4 RotateY(float angle) {
  FLOATMATRIX4 matRot;
  matRot.RotationY(3.141592653589793238462643383*angle/180.0);
  return matRot;
}
static FLOATMATRIX4 RotateZ(float angle) {
  FLOATMATRIX4 matRot;
  matRot.RotationZ(3.141592653589793238462643383*angle/180.0);
  return matRot;
}
static FLOATMATRIX4 Translate(float x, float y, float z) {
  FLOATMATRIX4 matTrans;
  matTrans.Translation(x, y, z);
  return matTrans;
}
static FLOATMATRIX4 Identity() {
  FLOATMATRIX4 matIdent;
  return matIdent;
}
static std::string VecToString(const FLOATVECTOR3& v) {
  std::ostringstream s;
  s << "{ " << v.x << ", " << v.y << ", " << v.z << "}";
  return s.str();
}
static FLOATMATRIX4 MulMatrices(const FLOATMATRIX4& a,
                                const FLOATMATRIX4& b) {
  return a * b;
}

void tuvok::registrar::matrix_math(std::shared_ptr<tuvok::LuaScripting>& ss) {
  ss->registerFunction(&RotateX, "matrix.rotateX",
                       "Constructs matrix rotated around x, N degrees.",
                       false);
  ss->registerFunction(&RotateY, "matrix.rotateY",
                       "Constructs matrix rotated around y, N degrees.",
                       false);
  ss->registerFunction(&RotateZ, "matrix.rotateZ",
                       "Constructs matrix rotated around z, N degrees.",
                       false);
  ss->registerFunction(&Translate, "matrix.translate",
                       "Constructs matrix translated by the given x,y,z",
                       false);
  ss->registerFunction(&Identity, "matrix.identity",
                       "Constructs identity matrix.", false);
  ss->registerFunction(&MulMatrices, "matrix.multiply",
                       "multiplies two matrices", false);
  ss->registerFunction(&VecToString, "strvec", "converts vec to string",
                       false);
}
