#ifndef NIFTYREG_H
#define NIFTYREG_H

typedef struct {
    nifti_image *image;
    mat44 *affine;
    int *completedIterations;
} aladin_result;

typedef struct {
    nifti_image *image;
    nifti_image *controlPoints;
    int *completedIterations;
} f3d_result;

extern "C"
SEXP reg_aladin_R (SEXP source, SEXP target, SEXP type, SEXP finalPrecision, SEXP nLevels, SEXP maxIterations, SEXP useBlockPercentage, SEXP finalInterpolation, SEXP targetMask, SEXP affineComponents, SEXP verbose);

extern "C"
SEXP reg_f3d_R (SEXP source, SEXP target, SEXP finalPrecision, SEXP nLevels, SEXP maxIterations, SEXP nBins, SEXP bendingEnergyWeight, SEXP jacobianWeight, SEXP finalSpacing, SEXP finalInterpolation, SEXP targetMask, SEXP affineComponents, SEXP initControl, SEXP verbose);

nifti_image * s4_image_to_struct (SEXP object);

bool reg_test_convergence (mat44 *updateMatrix);

nifti_image * copy_complete_nifti_image (nifti_image *source);

nifti_image * create_position_field (nifti_image *templateImage, bool twoDimRegistration);

mat44 * create_init_affine (nifti_image *sourceImage, nifti_image *targetImage);

aladin_result do_reg_aladin (nifti_image *sourceImage, nifti_image *targetImage, int type, int finalPrecision, int nLevels, int maxIterations, int useBlockPercentage, int finalInterpolation, nifti_image *targetMaskImage, mat44 *affineTransformation, bool verbose);

f3d_result do_reg_f3d (nifti_image *sourceImage, nifti_image *targetImage, int finalPrecision, int nLevels, int maxIterations, int finalInterpolation, nifti_image *targetMaskImage, nifti_image *controlPointImage, mat44 *affineTransformation, int nBins, float *spacing, float bendingEnergyWeight, float jacobianWeight, bool verbose);

#endif
