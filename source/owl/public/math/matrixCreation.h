/**
 * @file matrixCreation.h
 * @author Silmaen
 * @date 30/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrices.h"
#include "quaternion.h"

namespace owl::math {

/**
 * @brief Generate Identity matrix.
 * @tparam BaseType The internal Data type.
 * @tparam Dim The size of the matrix.
 * @return Identity Matrix.
 */
template<typename BaseType, std::size_t Dim>
constexpr auto identity() -> Matrix<BaseType, Dim, Dim> {
	Matrix<BaseType, Dim, Dim> result{};
	for (std::size_t idx = 0; idx < Dim; ++idx) result(idx, idx) = BaseType{1};
	return result;
}

/**
 * @brief Construct a 3x3 matrix based on the given quaternion.
 * @tparam BaseType The internal data type.
 * @param iQuaternion The quaternion.
 * @return The 3x3 matrix.
 */
template<typename BaseType>
constexpr auto toMat3(const Quaternion<BaseType>& iQuaternion) -> Matrix<BaseType, 3, 3> {
	Matrix<BaseType, 3, 3> result;
	constexpr BaseType one{1};
	constexpr BaseType two{2};
	const BaseType qxx(iQuaternion.x() * iQuaternion.x());
	const BaseType qyy(iQuaternion.y() * iQuaternion.y());
	const BaseType qzz(iQuaternion.z() * iQuaternion.z());
	const BaseType qxz(iQuaternion.x() * iQuaternion.z());
	const BaseType qxy(iQuaternion.x() * iQuaternion.y());
	const BaseType qyz(iQuaternion.y() * iQuaternion.z());
	const BaseType qwx(iQuaternion.w() * iQuaternion.x());
	const BaseType qwy(iQuaternion.w() * iQuaternion.y());
	const BaseType qwz(iQuaternion.w() * iQuaternion.z());
	result(0, 0) = one - two * (qyy + qzz);
	result(1, 0) = two * (qxy + qwz);
	result(2, 0) = two * (qxz - qwy);
	result(0, 1) = two * (qxy - qwz);
	result(1, 1) = one - two * (qxx + qzz);
	result(2, 1) = two * (qyz + qwx);
	result(0, 2) = two * (qxz + qwy);
	result(1, 2) = two * (qyz - qwx);
	result(2, 2) = one - two * (qxx + qyy);
	return result;
}

/**
 * @brief Construct a 4x4 transformation matrix based on the given quaternion.
 * @tparam BaseType The internal data type.
 * @param iQuaternion The quaternion.
 * @return The 4x4 matrix.
 */
template<typename BaseType>
constexpr auto toMat4(const Quaternion<BaseType>& iQuaternion) -> Matrix<BaseType, 4, 4> {
	Matrix<BaseType, 3, 3> res3 = toMat3(iQuaternion);
	Matrix<BaseType, 4, 4> result{};
	result(0, 0) = res3(0, 0);
	result(0, 1) = res3(0, 1);
	result(0, 2) = res3(0, 2);
	result(1, 0) = res3(1, 0);
	result(1, 1) = res3(1, 1);
	result(1, 2) = res3(1, 2);
	result(2, 0) = res3(2, 0);
	result(2, 1) = res3(2, 1);
	result(2, 2) = res3(2, 2);
	result(3, 3) = BaseType{1};
	return result;
}

/**
 * @brief Generate a perspective projection matrix.
 * @tparam BaseType The internal Data type.
 * @param iFovy The Y field of view.
 * @param iAspect The aspect ratio.
 * @param iZNear The nearest depth.
 * @param iZFar The farthest depth.
 * @return Perspective Matrix.
 */
template<typename BaseType>
constexpr auto perspective(BaseType iFovy, BaseType iAspect, BaseType iZNear, BaseType iZFar)
		-> Matrix<BaseType, 4, 4> {
	static constexpr BaseType one{1};
	static constexpr BaseType two{2};
	Matrix<BaseType, 4, 4> result{};
	const BaseType tanHalfFovy = std::tan(iFovy / two);
	result(0, 0) = one / (iAspect * tanHalfFovy);
	result(1, 1) = one / tanHalfFovy;
	result(2, 2) = -(iZFar + iZNear) / (iZFar - iZNear);
	result(3, 2) = -one;
	result(2, 3) = -(two * iZFar * iZNear) / (iZFar - iZNear);
	return result;
}

/**
 * @brief Construct an orthogonal projection matrix.
 * @tparam BaseType The internal data type.
 * @param iLeft The left limit.
 * @param iRight The right limit.
 * @param iBottom The bottom limit.
 * @param iTop The top limit.
 * @param iNear The nearest depth.
 * @param iFar The farthest depth.
 * @return The orthogonal matrix.
 */
template<typename BaseType>
constexpr auto ortho(const BaseType iLeft, const BaseType iRight, const BaseType iBottom, const BaseType iTop,
					 const BaseType iNear, const BaseType iFar) -> Matrix<BaseType, 4, 4> {
	static constexpr BaseType two{2};
	Matrix<BaseType, 4, 4> result{};
	result(0, 0) = two / (iRight - iLeft);
	result(1, 1) = two / (iTop - iBottom);
	result(2, 2) = -two / (iFar - iNear);
	result(0, 3) = -(iRight + iLeft) / (iRight - iLeft);
	result(1, 3) = -(iTop + iBottom) / (iTop - iBottom);
	result(2, 3) = -(iFar + iNear) / (iFar - iNear);
	result(3, 3) = BaseType{1};
	return result;
}

/**
 * @brief Create a translated matrix.
 * @tparam BaseType The internal data type.
 * @param iMatrix Matrix to transform.
 * @param iVector Translation vector.
 * @return The translated matrix.
 */
template<typename BaseType>
constexpr auto translate(const Matrix<BaseType, 4, 4>& iMatrix, const Vector<BaseType, 3>& iVector)
		-> Matrix<BaseType, 4, 4> {
	Matrix<BaseType, 4, 4> result(iMatrix);
	result.setColumn(3, iMatrix.column(0) * iVector[0] + iMatrix.column(1) * iVector[1] +
								iMatrix.column(2) * iVector[2] + iMatrix.column(3));
	return result;
}

/**
 * @brief Create a rotated matrix matrix.
 * @tparam BaseType The internal data type.
 * @param iMatrix The matrix to rotate.
 * @param iAngle The rotation angle in radians.
 * @param iAxis The rotation axis.
 * @return Rotated matrix.
 */
template<typename BaseType>
constexpr auto rotate(const Matrix<BaseType, 4, 4>& iMatrix, const BaseType iAngle, const Vector<BaseType, 3>& iAxis)
		-> Matrix<BaseType, 4, 4> {
	const BaseType c = std::cos(iAngle);
	const BaseType s = std::sin(iAngle);
	Vector<BaseType, 3> axis(iAxis.normalized());
	Vector<BaseType, 3> temp((BaseType(1) - c) * axis);
	Matrix<BaseType, 4, 4> rotate{};
	rotate(0, 0) = c + temp[0] * axis[0];
	rotate(1, 0) = temp[0] * axis[1] + s * axis[2];
	rotate(2, 0) = temp[0] * axis[2] - s * axis[1];
	rotate(0, 1) = temp[1] * axis[0] - s * axis[2];
	rotate(1, 1) = c + temp[1] * axis[1];
	rotate(2, 1) = temp[1] * axis[2] + s * axis[0];
	rotate(0, 2) = temp[2] * axis[0] + s * axis[1];
	rotate(1, 2) = temp[2] * axis[1] - s * axis[0];
	rotate(2, 2) = c + temp[2] * axis[2];
	Matrix<BaseType, 4, 4> result{};
	result.setColumn(0, iMatrix.column(0) * rotate(0, 0) + iMatrix.column(1) * rotate(1, 0) +
								iMatrix.column(2) * rotate(2, 0));
	result.setColumn(1, iMatrix.column(0) * rotate(0, 1) + iMatrix.column(1) * rotate(1, 1) +
								iMatrix.column(2) * rotate(2, 1));
	result.setColumn(2, iMatrix.column(0) * rotate(0, 2) + iMatrix.column(1) * rotate(1, 2) +
								iMatrix.column(2) * rotate(2, 2));
	result.setColumn(3, iMatrix.column(3));
	return result;
}

/**
 * @brief Create a scale matrix.
 * @tparam BaseType The internal data type.
 * @param iMatrix The matrix to transform.
 * @param iScale The scale vector.
 * @return The transformed matrix.
 */
template<typename BaseType>
constexpr auto scale(const Matrix<BaseType, 4, 4>& iMatrix, const Vector<BaseType, 3>& iScale)
		-> Matrix<BaseType, 4, 4> {
	Matrix<BaseType, 4, 4> result{};
	result.setColumn(0, iMatrix.column(0) * iScale[0]);
	result.setColumn(1, iMatrix.column(1) * iScale[1]);
	result.setColumn(2, iMatrix.column(2) * iScale[2]);
	result.setColumn(3, iMatrix.column(3));
	return result;
}

/**
 * @brief Create a sheared matrix.
 * @tparam BaseType The internal data type.
 * @param iMatrix The matrix to transform.
 * @param iPoint The center of the shearing.
 * @param iLambdaX Amount of shear in X.
 * @param iLambdaY Amount of shear in Y.
 * @param iLambdaZ Amount of shear in Z.
 * @return The transformed matrix.
 */
template<typename BaseType>
constexpr auto shear(const Matrix<BaseType, 4, 4>& iMatrix, const Vector<BaseType, 3>& iPoint,
					 const Vector<BaseType, 2>& iLambdaX, const Vector<BaseType, 2>& iLambdaY,
					 const Vector<BaseType, 2>& iLambdaZ) -> Matrix<BaseType, 4, 4> {

	Vector<BaseType, 3> pointLambda{(iLambdaX[0] + iLambdaX[1]), (iLambdaY[0] + iLambdaY[1]),
									(iLambdaZ[0] + iLambdaZ[1])};

	Matrix<BaseType, 4, 4> shear{1,
								 iLambdaY[0],
								 iLambdaZ[0],
								 0,
								 iLambdaX[0],
								 1,
								 iLambdaZ[1],
								 0,
								 iLambdaX[1],
								 iLambdaY[1],
								 1,
								 0,
								 -pointLambda[0] * iPoint[0],
								 -pointLambda[1] * iPoint[1],
								 -pointLambda[2] * iPoint[2],
								 1};

	Matrix<BaseType, 4, 4> result{};
	result.setColumn(0) = iMatrix.column(0) * shear(0, 0) + iMatrix.column(1) * shear(1, 0) +
						  iMatrix.column(2) * shear(2, 0) + iMatrix.column(3) * shear(3, 0);
	result.setColumn(1) = iMatrix.column(0) * shear(0, 1) + iMatrix.column(1) * shear(1, 1) +
						  iMatrix.column(2) * shear(2, 1) + iMatrix.column(3) * shear(3, 1);
	result.setColumn(2) = iMatrix.column(0) * shear(0, 2) + iMatrix.column(1) * shear(1, 2) +
						  iMatrix.column(2) * shear(2, 2) + iMatrix.column(3) * shear(3, 2);
	result.setColumn(3) = iMatrix.column(0) * shear(0, 3) + iMatrix.column(1) * shear(1, 3) +
						  iMatrix.column(2) * shear(2, 3) + iMatrix.column(3) * shear(3, 3);
	return result;
}

/**
 * @brief Inverse a matrix.
 * @tparam BaseType The internal data type.
 * @tparam Dim The matrix dimension.
 * @param iMatrix The matrix to inverse.
 * @return Inverse matrix.
 */
template<typename BaseType, size_t Dim>
constexpr auto inverse(const Matrix<BaseType, Dim, Dim>& iMatrix) -> Matrix<BaseType, Dim, Dim>
	requires(Dim == 2)
{
	BaseType determinant = iMatrix(0, 0) * iMatrix(1, 1) - iMatrix(1, 0) * iMatrix(0, 1);
	BaseType oneOverDeterminant = BaseType{1} / determinant;
	Matrix<BaseType, Dim, Dim> inverse;
	inverse(0, 0) = iMatrix(1, 1);
	inverse(1, 0) = -iMatrix(0, 1);
	inverse(0, 1) = -iMatrix(1, 0);
	inverse(1, 1) = +iMatrix(0, 0);
	inverse *= oneOverDeterminant;
	return inverse;
}

/**
 * @brief Inverse a matrix.
 * @tparam BaseType The internal data type.
 * @tparam Dim The matrix dimension.
 * @param iMatrix The matrix to inverse.
 * @return Inverse matrix.
 */
template<typename BaseType, size_t Dim>
constexpr auto inverse(const Matrix<BaseType, Dim, Dim>& iMatrix) -> Matrix<BaseType, Dim, Dim>
	requires(Dim == 3)
{
	BaseType determinant = iMatrix(0, 0) * (iMatrix(1, 1) * iMatrix(2, 2) - iMatrix(1, 2) * iMatrix(2, 1)) -
						   iMatrix(0, 1) * (iMatrix(1, 0) * iMatrix(2, 2) - iMatrix(1, 2) * iMatrix(2, 0)) +
						   iMatrix(0, 2) * (iMatrix(1, 0) * iMatrix(2, 1) - iMatrix(1, 1) * iMatrix(2, 0));
	BaseType oneOverDeterminant = BaseType{1} / determinant;
	Matrix<BaseType, Dim, Dim> inverse;
	inverse(0, 0) = +(iMatrix(1, 1) * iMatrix(2, 2) - iMatrix(1, 2) * iMatrix(2, 1));
	inverse(0, 1) = -(iMatrix(0, 1) * iMatrix(2, 2) - iMatrix(0, 2) * iMatrix(2, 1));
	inverse(0, 2) = +(iMatrix(0, 1) * iMatrix(1, 2) - iMatrix(0, 2) * iMatrix(1, 1));
	inverse(1, 0) = -(iMatrix(1, 0) * iMatrix(2, 2) - iMatrix(1, 2) * iMatrix(2, 0));
	inverse(1, 1) = +(iMatrix(0, 0) * iMatrix(2, 2) - iMatrix(0, 2) * iMatrix(2, 0));
	inverse(1, 2) = -(iMatrix(0, 0) * iMatrix(1, 2) - iMatrix(0, 2) * iMatrix(1, 0));
	inverse(2, 0) = +(iMatrix(1, 0) * iMatrix(2, 1) - iMatrix(1, 1) * iMatrix(2, 0));
	inverse(2, 1) = -(iMatrix(0, 0) * iMatrix(2, 1) - iMatrix(0, 1) * iMatrix(2, 0));
	inverse(2, 2) = +(iMatrix(0, 0) * iMatrix(1, 1) - iMatrix(0, 1) * iMatrix(1, 0));
	inverse *= oneOverDeterminant;
	return inverse;
}

/**
 * @brief Inverse a matrix.
 * @tparam BaseType The internal data type.
 * @tparam Dim The matrix dimension.
 * @param iMatrix The matrix to inverse.
 * @return Inverse matrix.
 */
template<typename BaseType, size_t Dim>
constexpr auto inverse(const Matrix<BaseType, Dim, Dim>& iMatrix) -> Matrix<BaseType, Dim, Dim>
	requires(Dim == 4)
{
	const BaseType coef00 = iMatrix(2, 2) * iMatrix(3, 3) - iMatrix(2, 3) * iMatrix(3, 2);
	const BaseType coef02 = iMatrix(2, 1) * iMatrix(3, 3) - iMatrix(2, 3) * iMatrix(3, 1);
	const BaseType coef03 = iMatrix(2, 1) * iMatrix(3, 2) - iMatrix(2, 2) * iMatrix(3, 1);

	const BaseType coef04 = iMatrix(1, 2) * iMatrix(3, 3) - iMatrix(1, 3) * iMatrix(3, 2);
	const BaseType coef06 = iMatrix(1, 1) * iMatrix(3, 3) - iMatrix(1, 3) * iMatrix(3, 1);
	const BaseType coef07 = iMatrix(1, 1) * iMatrix(3, 2) - iMatrix(1, 2) * iMatrix(3, 1);

	const BaseType coef08 = iMatrix(1, 2) * iMatrix(2, 3) - iMatrix(1, 3) * iMatrix(2, 2);
	const BaseType coef10 = iMatrix(1, 1) * iMatrix(2, 3) - iMatrix(1, 3) * iMatrix(2, 1);
	const BaseType coef11 = iMatrix(1, 1) * iMatrix(2, 2) - iMatrix(1, 2) * iMatrix(2, 1);

	const BaseType coef12 = iMatrix(0, 2) * iMatrix(3, 3) - iMatrix(0, 3) * iMatrix(3, 2);
	const BaseType coef14 = iMatrix(0, 1) * iMatrix(3, 3) - iMatrix(0, 3) * iMatrix(3, 1);
	const BaseType coef15 = iMatrix(0, 1) * iMatrix(3, 2) - iMatrix(0, 2) * iMatrix(3, 1);

	const BaseType coef16 = iMatrix(0, 2) * iMatrix(2, 3) - iMatrix(0, 3) * iMatrix(2, 2);
	const BaseType coef18 = iMatrix(0, 1) * iMatrix(2, 3) - iMatrix(0, 3) * iMatrix(2, 1);
	const BaseType coef19 = iMatrix(0, 1) * iMatrix(2, 2) - iMatrix(0, 2) * iMatrix(2, 1);

	const BaseType coef20 = iMatrix(0, 2) * iMatrix(1, 3) - iMatrix(0, 3) * iMatrix(1, 2);
	const BaseType coef22 = iMatrix(0, 1) * iMatrix(1, 3) - iMatrix(0, 3) * iMatrix(1, 1);
	const BaseType coef23 = iMatrix(0, 1) * iMatrix(1, 2) - iMatrix(0, 2) * iMatrix(1, 1);

	const Vector<BaseType, Dim> fac0{coef00, coef00, coef02, coef03};
	const Vector<BaseType, Dim> fac1{coef04, coef04, coef06, coef07};
	const Vector<BaseType, Dim> fac2{coef08, coef08, coef10, coef11};
	const Vector<BaseType, Dim> fac3{coef12, coef12, coef14, coef15};
	const Vector<BaseType, Dim> fac4{coef16, coef16, coef18, coef19};
	const Vector<BaseType, Dim> fac5{coef20, coef20, coef22, coef23};

	const Vector<BaseType, Dim> vec0{iMatrix(0, 1), iMatrix(0, 0), iMatrix(0, 0), iMatrix(0, 0)};
	const Vector<BaseType, Dim> vec1{iMatrix(1, 1), iMatrix(1, 0), iMatrix(1, 0), iMatrix(1, 0)};
	const Vector<BaseType, Dim> vec2{iMatrix(2, 1), iMatrix(2, 0), iMatrix(2, 0), iMatrix(2, 0)};
	const Vector<BaseType, Dim> vec3{iMatrix(3, 1), iMatrix(3, 0), iMatrix(3, 0), iMatrix(3, 0)};

	Vector<BaseType, Dim> inv0{};
	Vector<BaseType, Dim> inv1{};
	Vector<BaseType, Dim> inv2{};
	Vector<BaseType, Dim> inv3{};
	for (size_t it = 0; it < Dim; ++it) {
		inv0[it] = vec1[it] * fac0[it] - vec2[it] * fac1[it] + vec3[it] * fac2[it];
		inv1[it] = vec0[it] * fac0[it] - vec2[it] * fac3[it] + vec3[it] * fac4[it];
		inv2[it] = vec0[it] * fac1[it] - vec1[it] * fac3[it] + vec3[it] * fac5[it];
		inv3[it] = vec0[it] * fac2[it] - vec1[it] * fac4[it] + vec2[it] * fac5[it];
	}

	Matrix<BaseType, Dim, Dim> inverse{inv0[0], -inv0[1], inv0[2], -inv0[3], -inv1[0], inv1[1], -inv1[2], inv1[3],
									   inv2[0], -inv2[1], inv2[2], -inv2[3], -inv3[0], inv3[1], -inv3[2], inv3[3]};

	Vector<BaseType, Dim> row0 = inverse.row(0);
	BaseType determinant =
			iMatrix(0, 0) * row0[0] + iMatrix(1, 0) * row0[1] + iMatrix(2, 0) * row0[2] + iMatrix(3, 0) * row0[3];
	BaseType oneOverDeterminant = BaseType{1} / determinant;
	inverse *= oneOverDeterminant;
	return inverse;
}

}// namespace owl::math
