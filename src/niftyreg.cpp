// Registration types (degrees of freedom) - these constants have to have these values
#define RIGID 0
#define AFFINE 1

// Precision levels for final interpolation
#define INTERP_PREC_SOURCE 0
#define INTERP_PREC_DOUBLE 1

// Working precision for the source and target images: float and double are valid for the NiftyReg code
#define PRECISION_TYPE double

// Convergence criterion
#define CONVERGENCE_EPS 0.00001

#define JH_TRI 0
#define JH_PARZEN_WIN 1
#define JH_PW_APPROX 2

#define FOLDING_CORRECTION_STEP 20

#include "_reg_resampling.h"
#include "_reg_globalTransformation.h"
#include "_reg_blockMatching.h"
#include "_reg_tools.h"
#include "_reg_f3d.h"

#include <R.h>
#include <Rdefines.h>
#include <Rinternals.h>

#include "niftyreg.h"
#include "substitutions.h"

extern "C"
SEXP reg_aladin_R (SEXP source, SEXP target, SEXP type, SEXP finalPrecision, SEXP nLevels, SEXP maxIterations, SEXP useBlockPercentage, SEXP finalInterpolation, SEXP targetMask, SEXP affineComponents, SEXP verbose)
{
    int i, j, levels = *(INTEGER(nLevels));
    SEXP returnValue, data, completedIterations;
    
    bool affineProvided = !isNull(affineComponents);
    
    nifti_image *sourceImage = s4_image_to_struct(source);
    nifti_image *targetImage = s4_image_to_struct(target);
    nifti_image *targetMaskImage = NULL;
    mat44 *affineTransformation = NULL;
    
    if (!isNull(targetMask))
        targetMaskImage = s4_image_to_struct(targetMask);
    
    if (affineProvided)
    {
        affineTransformation = (mat44 *) calloc(1, sizeof(mat44));
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
                affineTransformation->m[i][j] = (float) REAL(affineComponents)[(j*4)+i];
        }
    }
    
    int regType = (strcmp(CHAR(STRING_ELT(type,0)),"rigid")==0 ? RIGID : AFFINE);
    int precisionType = (strcmp(CHAR(STRING_ELT(finalPrecision,0)),"source")==0 ? INTERP_PREC_SOURCE : INTERP_PREC_DOUBLE);
    
    aladin_result result = do_reg_aladin(sourceImage, targetImage, regType, precisionType, *(INTEGER(nLevels)), *(INTEGER(maxIterations)), *(INTEGER(useBlockPercentage)), *(INTEGER(finalInterpolation)), targetMaskImage, affineTransformation, (*(INTEGER(verbose)) == 1));
    
    PROTECT(returnValue = NEW_LIST(3));
    
    if (result.image->datatype == DT_INT32)
    {
        // Integer-valued data went in, and precision must be "source"
        PROTECT(data = NEW_INTEGER((R_len_t) result.image->nvox));
        for (size_t i = 0; i < result.image->nvox; i++)
            INTEGER(data)[i] = ((int *) result.image->data)[i];
    }
    else
    {
        // All other cases
        PROTECT(data = NEW_NUMERIC((R_len_t) result.image->nvox));
        for (size_t i = 0; i < result.image->nvox; i++)
            REAL(data)[i] = ((double *) result.image->data)[i];
    }
    
    SET_ELEMENT(returnValue, 0, data);
    
    if (!affineProvided)
        PROTECT(affineComponents = NEW_NUMERIC(16));
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
            REAL(affineComponents)[(j*4)+i] = (double) result.affine->m[i][j];
    }
    
    SET_ELEMENT(returnValue, 1, affineComponents);
    
    PROTECT(completedIterations = NEW_INTEGER(levels));
    for (i = 0; i < levels; i++)
        INTEGER(completedIterations)[i] = result.completedIterations[i];
    
    SET_ELEMENT(returnValue, 2, completedIterations);
    
    nifti_image_free(sourceImage);
    nifti_image_free(targetImage);
    if (targetMaskImage != NULL)
        nifti_image_free(targetMaskImage);
    nifti_image_free(result.image);
    free(result.affine);
    free(result.completedIterations);
    
    UNPROTECT(affineProvided ? 3 : 4);
    
    return returnValue;
}

extern "C"
SEXP reg_f3d_R (SEXP source, SEXP target, SEXP finalPrecision, SEXP nLevels, SEXP maxIterations, SEXP nBins, SEXP bendingEnergyWeight, SEXP jacobianWeight, SEXP finalSpacing, SEXP finalInterpolation, SEXP targetMask, SEXP affineComponents, SEXP initControl, SEXP verbose)
{
    int i, j, levels = *(INTEGER(nLevels));
    SEXP returnValue, data, controlPoints, completedIterations, xform;
    
    bool affineProvided = !isNull(affineComponents);
    
    nifti_image *sourceImage = s4_image_to_struct(source);
    nifti_image *targetImage = s4_image_to_struct(target);
    nifti_image *targetMaskImage = NULL;
    nifti_image *controlPointImage = NULL;
    mat44 *affineTransformation = NULL;
    
    if (!isNull(targetMask))
        targetMaskImage = s4_image_to_struct(targetMask);
    if (!isNull(initControl))
        controlPointImage = s4_image_to_struct(initControl);
    
    if (affineProvided)
    {
        affineTransformation = (mat44 *) calloc(1, sizeof(mat44));
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
                affineTransformation->m[i][j] = (float) REAL(affineComponents)[(j*4)+i];
        }
    }
    
    float spacing[3];
    for (i = 0; i < 3; i++)
        spacing[i] = (float) REAL(finalSpacing)[i];
    
    int precisionType = (strcmp(CHAR(STRING_ELT(finalPrecision,0)),"source")==0 ? INTERP_PREC_SOURCE : INTERP_PREC_DOUBLE);
    
    f3d_result result = do_reg_f3d(sourceImage, targetImage, precisionType, *(INTEGER(nLevels)), *(INTEGER(maxIterations)), *(INTEGER(finalInterpolation)), targetMaskImage, controlPointImage, affineTransformation, *(INTEGER(nBins)), spacing, (float) *(REAL(bendingEnergyWeight)), (float) *(REAL(jacobianWeight)), (*(INTEGER(verbose)) == 1));
    
    PROTECT(returnValue = NEW_LIST(4));
    
    if (result.image->datatype == DT_INT32)
    {
        // Integer-valued data went in, and precision must be "source"
        PROTECT(data = NEW_INTEGER((R_len_t) result.image->nvox));
        for (size_t i = 0; i < result.image->nvox; i++)
            INTEGER(data)[i] = ((int *) result.image->data)[i];
    }
    else
    {
        // All other cases
        PROTECT(data = NEW_NUMERIC((R_len_t) result.image->nvox));
        for (size_t i = 0; i < result.image->nvox; i++)
            REAL(data)[i] = ((double *) result.image->data)[i];
    }
    
    SET_ELEMENT(returnValue, 0, data);
    
    PROTECT(controlPoints = NEW_NUMERIC((R_len_t) result.controlPoints->nvox));
    for (size_t i = 0; i < result.controlPoints->nvox; i++)
        REAL(controlPoints)[i] = ((double *) result.controlPoints->data)[i];
    
    SET_ELEMENT(returnValue, 1, controlPoints);
    
    PROTECT(completedIterations = NEW_INTEGER((R_len_t) levels));
    for (i = 0; i < levels; i++)
        INTEGER(completedIterations)[i] = result.completedIterations[i];
    
    SET_ELEMENT(returnValue, 2, completedIterations);
    
    PROTECT(xform = NEW_NUMERIC(21));
    REAL(xform)[0] = (double) result.controlPoints->qform_code;
    REAL(xform)[1] = (double) result.controlPoints->sform_code;
    REAL(xform)[2] = (double) result.controlPoints->quatern_b;
    REAL(xform)[3] = (double) result.controlPoints->quatern_c;
    REAL(xform)[4] = (double) result.controlPoints->quatern_d;
    REAL(xform)[5] = (double) result.controlPoints->qoffset_x;
    REAL(xform)[6] = (double) result.controlPoints->qoffset_y;
    REAL(xform)[7] = (double) result.controlPoints->qoffset_z;
    REAL(xform)[8] = (double) result.controlPoints->qfac;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 4; j++)
            REAL(xform)[(i*4)+j+9] = (double) result.controlPoints->sto_xyz.m[i][j];
    }
    
    SET_ELEMENT(returnValue, 3, xform);
    
    nifti_image_free(sourceImage);
    nifti_image_free(targetImage);
    if (targetMaskImage != NULL)
        nifti_image_free(targetMaskImage);
    if (controlPointImage != NULL)
        nifti_image_free(controlPointImage);
    nifti_image_free(result.image);
    nifti_image_free(result.controlPoints);
    free(result.completedIterations);
    if (affineProvided || isNull(initControl))
        free(affineTransformation);
    
    UNPROTECT(5);
    
    return returnValue;
}

// Convert an S4 "nifti" object, as defined in the oro.nifti package, to a "nifti_image" struct
nifti_image * s4_image_to_struct (SEXP object)
{
    int i;
    nifti_1_header header;
    
    header.sizeof_hdr = 348;
    
    for (i = 0; i < 8; i++)
        header.dim[i] = (short) INTEGER(GET_SLOT(object, install("dim_")))[i];
    
    header.intent_p1 = (float) *(REAL(GET_SLOT(object, install("intent_p1"))));
    header.intent_p2 = (float) *(REAL(GET_SLOT(object, install("intent_p2"))));
    header.intent_p3 = (float) *(REAL(GET_SLOT(object, install("intent_p3"))));
    header.intent_code = (short) *(INTEGER(GET_SLOT(object, install("intent_code"))));
    
    header.datatype = (short) *(INTEGER(GET_SLOT(object, install("datatype"))));
    header.bitpix = (short) *(INTEGER(GET_SLOT(object, install("bitpix"))));
    
    header.slice_start = (short) *(INTEGER(GET_SLOT(object, install("slice_start"))));
    header.slice_end = (short) *(INTEGER(GET_SLOT(object, install("slice_end"))));
    header.slice_code = (char) *(INTEGER(GET_SLOT(object, install("slice_code"))));
    header.slice_duration = (float) *(REAL(GET_SLOT(object, install("slice_duration"))));
    
    for (i = 0; i < 8; i++) 
        header.pixdim[i] = (float) REAL(GET_SLOT(object, install("pixdim")))[i];
    header.xyzt_units = (char) *(INTEGER(GET_SLOT(object, install("xyzt_units"))));
    
    header.vox_offset = (float) *(REAL(GET_SLOT(object, install("vox_offset"))));
    
    header.scl_slope = (float) *(REAL(GET_SLOT(object, install("scl_slope"))));
    header.scl_inter = (float) *(REAL(GET_SLOT(object, install("scl_inter"))));
    header.toffset = (float) *(REAL(GET_SLOT(object, install("toffset"))));
    
    header.cal_max = (float) *(REAL(GET_SLOT(object, install("cal_max"))));
    header.cal_min = (float) *(REAL(GET_SLOT(object, install("cal_min"))));
    header.glmax = header.glmin = 0;
    
    strcpy(header.descrip, CHAR(STRING_ELT(GET_SLOT(object, install("descrip")),0)));
    strcpy(header.aux_file, CHAR(STRING_ELT(GET_SLOT(object, install("aux_file")),0)));
    strcpy(header.intent_name, CHAR(STRING_ELT(GET_SLOT(object, install("intent_name")),0)));
    strcpy(header.magic, CHAR(STRING_ELT(GET_SLOT(object, install("magic")),0)));
    
    header.qform_code = (short) *(INTEGER(GET_SLOT(object, install("qform_code"))));
    header.sform_code = (short) *(INTEGER(GET_SLOT(object, install("sform_code"))));
    
    header.quatern_b = (float) *(REAL(GET_SLOT(object, install("quatern_b"))));
    header.quatern_c = (float) *(REAL(GET_SLOT(object, install("quatern_c"))));
    header.quatern_d = (float) *(REAL(GET_SLOT(object, install("quatern_d"))));
    header.qoffset_x = (float) *(REAL(GET_SLOT(object, install("qoffset_x"))));
    header.qoffset_y = (float) *(REAL(GET_SLOT(object, install("qoffset_y"))));
    header.qoffset_z = (float) *(REAL(GET_SLOT(object, install("qoffset_z"))));
    
    for (i = 0; i < 4; i++)
    {
        header.srow_x[i] = (float) REAL(GET_SLOT(object, install("srow_x")))[i];
        header.srow_y[i] = (float) REAL(GET_SLOT(object, install("srow_y")))[i];
        header.srow_z[i] = (float) REAL(GET_SLOT(object, install("srow_z")))[i];
    }
    
    if (header.datatype == DT_UINT8 || header.datatype == DT_INT16 || header.datatype == DT_INT32 || header.datatype == DT_INT8 || header.datatype == DT_UINT16 || header.datatype == DT_UINT32)
        header.datatype = DT_INT32;
    else if (header.datatype == DT_FLOAT32 || header.datatype == DT_FLOAT64)
        header.datatype = DT_FLOAT64;  // This assumes that sizeof(double) == 8
    else
    {
        error("Data type %d is not supported", header.datatype);
        return NULL;
    }
    
    nifti_image *image = nifti_convert_nhdr2nim(header, NULL);
    
    size_t dataSize = nifti_get_volsize(image);
    image->data = calloc(1, dataSize);
    if (header.datatype == DT_INT32)
        memcpy(image->data, INTEGER(GET_SLOT(object, install(".Data"))), dataSize);
    else
        memcpy(image->data, REAL(GET_SLOT(object, install(".Data"))), dataSize);
    
    return image;
}

// Does the specified update matrix represent convergence?
bool reg_test_convergence (mat44 *updateMatrix)
{
    bool convergence = true;
    float referenceValue;
    
    // The fourth row is always [0 0 0 1] so we don't test it
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            // The diagonal represents scale factors, so 1 indicates no change
            referenceValue = (i==j ? 1.0f : 0.0f);
            if ((fabsf(updateMatrix->m[i][j]) - referenceValue) > CONVERGENCE_EPS)
                convergence = false;
        }
    }

    return convergence;
}

nifti_image * copy_complete_nifti_image (nifti_image *source)
{
    size_t dataSize = nifti_get_volsize(source);
    nifti_image *destination = nifti_copy_nim_info(source);
    destination->data = calloc(1, dataSize);
    memcpy(destination->data, source->data, dataSize);
    
    return destination;
}

nifti_image * create_position_field (nifti_image *templateImage, bool twoDimRegistration)
{
    nifti_image *positionFieldImage = nifti_copy_nim_info(templateImage);
    
    positionFieldImage->dim[0] = positionFieldImage->ndim = 5;
    positionFieldImage->dim[1] = positionFieldImage->nx = templateImage->nx;
    positionFieldImage->dim[2] = positionFieldImage->ny = templateImage->ny;
    positionFieldImage->dim[3] = positionFieldImage->nz = templateImage->nz;
    positionFieldImage->dim[4] = positionFieldImage->nt = 1;
    positionFieldImage->pixdim[4] = positionFieldImage->dt = 1.0;
    positionFieldImage->dim[5] = positionFieldImage->nu = (twoDimRegistration ? 2 : 3);
    positionFieldImage->pixdim[5] = positionFieldImage->du = 1.0;
    positionFieldImage->dim[6] = positionFieldImage->nv = 1;
    positionFieldImage->pixdim[6] = positionFieldImage->dv = 1.0;
    positionFieldImage->dim[7] = positionFieldImage->nw = 1;
    positionFieldImage->pixdim[7] = positionFieldImage->dw = 1.0;
    positionFieldImage->nvox = positionFieldImage->nx * positionFieldImage->ny * positionFieldImage->nz * positionFieldImage->nt * positionFieldImage->nu;
    positionFieldImage->datatype = (sizeof(PRECISION_TYPE)==4 ? NIFTI_TYPE_FLOAT32 : NIFTI_TYPE_FLOAT64);
    positionFieldImage->nbyper = sizeof(PRECISION_TYPE);
    positionFieldImage->data = calloc(positionFieldImage->nvox, positionFieldImage->nbyper);
    
    return positionFieldImage;
}

mat44 * create_init_affine (nifti_image *sourceImage, nifti_image *targetImage)
{
    mat44 *affineTransformation = (mat44 *) calloc(1, sizeof(mat44));
    for (int i = 0; i < 4; i++)
        affineTransformation->m[i][i] = 1.0;

    mat44 *sourceMatrix, *targetMatrix;
    float sourceCentre[3], targetCentre[3], sourceRealPosition[3], targetRealPosition[3];

	if (sourceImage->sform_code>0)
		sourceMatrix = &(sourceImage->sto_xyz);
	else
	    sourceMatrix = &(sourceImage->qto_xyz);
	if (targetImage->sform_code>0)
		targetMatrix = &(targetImage->sto_xyz);
	else
	    targetMatrix = &(targetImage->qto_xyz);

	sourceCentre[0] = (float) (sourceImage->nx) / 2.0f;
	sourceCentre[1] = (float) (sourceImage->ny) / 2.0f;
	sourceCentre[2] = (float) (sourceImage->nz) / 2.0f;

	targetCentre[0] = (float) (targetImage->nx) / 2.0f;
	targetCentre[1] = (float) (targetImage->ny) / 2.0f;
	targetCentre[2] = (float) (targetImage->nz) / 2.0f;

	reg_mat44_mul(sourceMatrix, sourceCentre, sourceRealPosition);
	reg_mat44_mul(targetMatrix, targetCentre, targetRealPosition);

	// Use origins to initialise translation elements
	affineTransformation->m[0][3] = sourceRealPosition[0] - targetRealPosition[0];
	affineTransformation->m[1][3] = sourceRealPosition[1] - targetRealPosition[1];
	affineTransformation->m[2][3] = sourceRealPosition[2] - targetRealPosition[2];
	
    return affineTransformation;
}

// Run the "aladin" registration algorithm
aladin_result do_reg_aladin (nifti_image *sourceImage, nifti_image *targetImage, int type, int finalPrecision, int nLevels, int maxIterations, int useBlockPercentage, int finalInterpolation, nifti_image *targetMaskImage, mat44 *affineTransformation, bool verbose)
{
    int i;
    bool usingTargetMask = (targetMaskImage != NULL);
    bool twoDimRegistration = (sourceImage->nz == 1 || targetImage->nz == 1);
    float sourceBGValue = 0.0;
    nifti_image *resultImage, *positionFieldImage = NULL;
    
    int *completedIterations = (int *) calloc(nLevels, sizeof(int));
    
    // Initial affine matrix is the identity
    if (affineTransformation == NULL)
        affineTransformation = create_init_affine(sourceImage, targetImage);
    
    // Binarise the mask image
    if (usingTargetMask)
        reg_tool_binarise_image(targetMaskImage);
    
    for (int level = 0; level < nLevels; level++)
    {
        nifti_image *sourceImageCopy = copy_complete_nifti_image(sourceImage);
        nifti_image *targetImageCopy = copy_complete_nifti_image(targetImage);
        nifti_image *targetMaskImageCopy = NULL;
        if (usingTargetMask)
            targetMaskImageCopy = copy_complete_nifti_image(targetMaskImage);
        
        reg_changeDatatype<PRECISION_TYPE>(sourceImageCopy);
        reg_changeDatatype<PRECISION_TYPE>(targetImageCopy);
        
        for (int l = level; l < nLevels-1; l++)
        {
            int ratio = (int) powf(2.0f, l+1.0f);

            bool sourceDownsampleAxis[8] = {true,true,true,true,true,true,true,true};
            if ((sourceImage->nx/ratio) < 32) sourceDownsampleAxis[1] = false;
            if ((sourceImage->ny/ratio) < 32) sourceDownsampleAxis[2] = false;
            if ((sourceImage->nz/ratio) < 32) sourceDownsampleAxis[3] = false;
            reg_downsampleImage<PRECISION_TYPE>(sourceImageCopy, 1, sourceDownsampleAxis);

            bool targetDownsampleAxis[8] = {true,true,true,true,true,true,true,true};
            if ((targetImage->nx/ratio) < 32) targetDownsampleAxis[1] = false;
            if ((targetImage->ny/ratio) < 32) targetDownsampleAxis[2] = false;
            if ((targetImage->nz/ratio) < 32) targetDownsampleAxis[3] = false;
            reg_downsampleImage<PRECISION_TYPE>(targetImageCopy, 1, targetDownsampleAxis);

            if (usingTargetMask)
                reg_downsampleImage<PRECISION_TYPE>(targetMaskImageCopy, 0, targetDownsampleAxis);
        }
        
        int activeVoxelNumber = 0;
        int *targetMask = (int *) calloc(targetImageCopy->nvox, sizeof(int));
        if (usingTargetMask)
        {
            reg_tool_binaryImage2int(targetMaskImageCopy, targetMask, activeVoxelNumber);
            nifti_image_free(targetMaskImageCopy);
        }
        else
        {
            for (unsigned int j = 0; j < targetImageCopy->nvox; j++)
                targetMask[j] = j;
            activeVoxelNumber = targetImageCopy->nvox;
        }
        
        // Allocate the deformation field image
        positionFieldImage = create_position_field(targetImageCopy, twoDimRegistration);
        
        // Allocate the result image
        resultImage = nifti_copy_nim_info(targetImageCopy);
        resultImage->datatype = sourceImageCopy->datatype;
        resultImage->nbyper = sourceImageCopy->nbyper;
        resultImage->data = calloc(1, nifti_get_volsize(resultImage));

        // Initialise the block matching - all the blocks are used during the first level
        _reg_blockMatchingParam blockMatchingParams;
        initialise_block_matching_method(targetImageCopy, &blockMatchingParams, (level==0 ? 100 : useBlockPercentage), 50, targetMask, 0);

        if (verbose)
        {
            // Display some parameters specific to the current level
            Rprintf("Current level %i / %i\n", level+1, nLevels);
            Rprintf("Target image size: \t%ix%ix%i voxels\t%gx%gx%g mm\n", targetImageCopy->nx, targetImageCopy->ny, targetImageCopy->nz, targetImageCopy->dx, targetImageCopy->dy, targetImageCopy->dz);
            Rprintf("Source image size: \t%ix%ix%i voxels\t%gx%gx%g mm\n", sourceImageCopy->nx, sourceImageCopy->ny, sourceImageCopy->nz, sourceImageCopy->dx, sourceImageCopy->dy, sourceImageCopy->dz);
            if (twoDimRegistration)
                Rprintf("Block size = [4 4 1]\n");
            else
                Rprintf("Block size = [4 4 4]\n");
            Rprintf("Block number = [%i %i %i]\n", blockMatchingParams.blockNumber[0], blockMatchingParams.blockNumber[1], blockMatchingParams.blockNumber[2]);
            Rprintf("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
            reg_mat44_disp(affineTransformation, (char *) "Initial affine transformation");
        }

        int nLoops = ((type==AFFINE && level==0) ? 2 : 1);
        int currentType, iteration;
        mat44 updateAffineMatrix;
        
        for (i = 0; i < nLoops; i++)
        {
            // The first optimisation is rigid even if the final scope is affine
            currentType = ((i==0 && nLoops==2) ? RIGID : type);
            iteration = 0;
            
            // Twice as many iterations are performed during the first level
            while (iteration < (level==0 ? 2*maxIterations : maxIterations))
            {
                // Compute the affine transformation deformation field
                reg_affine_positionField(affineTransformation, targetImageCopy, positionFieldImage);
                
                // Resample the source image
                reg_resampleSourceImage(targetImageCopy, sourceImageCopy, resultImage, positionFieldImage, targetMask, 1, sourceBGValue);
                
                // Compute the correspondances between blocks - this is the expensive bit
                block_matching_method<PRECISION_TYPE>(targetImageCopy, resultImage, &blockMatchingParams, targetMask);
                
                // Optimise the update matrix
                optimize(&blockMatchingParams, &updateAffineMatrix, currentType);
                
                // Update the affine transformation matrix
                *affineTransformation = reg_mat44_mul(affineTransformation, &(updateAffineMatrix));
                    
                if (reg_test_convergence(&updateAffineMatrix))
                    break;
                
                iteration++;
            }
        }
        
        completedIterations[level] = iteration;

        free(targetMask);
        nifti_image_free(resultImage);
        nifti_image_free(targetImageCopy);
        nifti_image_free(sourceImageCopy);
        
        if (level < (nLevels - 1))
            nifti_image_free(positionFieldImage);
        
        if (verbose)
        {
            reg_mat44_disp(affineTransformation, (char *)"Final affine transformation");
            Rprintf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n\n");
        }
    }
    
    if (nLevels == 0)
        positionFieldImage = create_position_field(targetImage, twoDimRegistration);
    
    // The corresponding deformation field is evaluated and saved
    reg_affine_positionField(affineTransformation, targetImage, positionFieldImage);
    
    // The source data type is changed for precision if requested
    if (finalPrecision == INTERP_PREC_DOUBLE)
        reg_changeDatatype<double>(sourceImage);

    // The result image is resampled using a cubic spline interpolation
    resultImage = nifti_copy_nim_info(targetImage);
    resultImage->cal_min = sourceImage->cal_min;
    resultImage->cal_max = sourceImage->cal_max;
    resultImage->scl_slope = sourceImage->scl_slope;
    resultImage->scl_inter = sourceImage->scl_inter;
    resultImage->datatype = sourceImage->datatype;
    resultImage->nbyper = sourceImage->nbyper;
    resultImage->data = calloc(resultImage->nvox, resultImage->nbyper);
    reg_resampleSourceImage(targetImage, sourceImage, resultImage, positionFieldImage, NULL, finalInterpolation, sourceBGValue);
    
    nifti_image_free(positionFieldImage);
    
    aladin_result result;
    result.image = resultImage;
    result.affine = affineTransformation;
    result.completedIterations = completedIterations;
    
    return result;
}

f3d_result do_reg_f3d (nifti_image *sourceImage, nifti_image *targetImage, int finalPrecision, int nLevels, int maxIterations, int finalInterpolation, nifti_image *targetMaskImage, nifti_image *controlPointImage, mat44 *affineTransformation, int nBins, float *spacing, float bendingEnergyWeight, float jacobianWeight, bool verbose)
{
    if (controlPointImage == NULL && affineTransformation == NULL)
        affineTransformation = create_init_affine(sourceImage, targetImage);
    
    // Binarise the mask image
    if (targetMaskImage != NULL)
        reg_tool_binarise_image(targetMaskImage);
    
    // The source data type is changed for precision if requested
    if (finalPrecision == INTERP_PREC_DOUBLE)
        reg_changeDatatype<double>(sourceImage);
    
    f3d_result result;
    
    if (nLevels == 0)
    {
        // Allocate and set completed iterations (nothing will be done so this is zero)
        int *completedIterations = (int *) calloc(1, sizeof(int));
        *completedIterations = 0;
        
        reg_checkAndCorrectDimension(controlPointImage);
        
        // Allocate a deformation field image
        nifti_image *deformationFieldImage = create_position_field(targetImage, targetImage->nz <= 1);
        
        // Calculate deformation field from the control point image
        if(controlPointImage->pixdim[5] > 1)
            reg_getDeformationFieldFromVelocityGrid(controlPointImage, deformationFieldImage, NULL, false);
        else
            reg_spline_getDeformationField(controlPointImage, targetImage, deformationFieldImage, NULL, false, true);
        
        // Allocate result image
        nifti_image *resultImage = nifti_copy_nim_info(targetImage);
        resultImage->dim[0] = resultImage->ndim = sourceImage->dim[0];
        resultImage->dim[4] = resultImage->nt = sourceImage->dim[4];
        resultImage->cal_min = sourceImage->cal_min;
        resultImage->cal_max = sourceImage->cal_max;
        resultImage->scl_slope = sourceImage->scl_slope;
        resultImage->scl_inter = sourceImage->scl_inter;
        resultImage->datatype = sourceImage->datatype;
        resultImage->nbyper = sourceImage->nbyper;
        resultImage->nvox = resultImage->dim[1] * resultImage->dim[2] * resultImage->dim[3] * resultImage->dim[4];
        resultImage->data = (void *) calloc(resultImage->nvox, resultImage->nbyper);
        
        // Resample source image to target space
        reg_resampleSourceImage(targetImage, sourceImage, resultImage, deformationFieldImage, NULL, finalInterpolation, 0);
        
        // Free deformation field image
        nifti_image_free(deformationFieldImage);
        
        result.image = resultImage;
        result.controlPoints = copy_complete_nifti_image(controlPointImage);
        result.completedIterations = completedIterations;
    }
    else
    {
        // Create the object encapsulating the registration
        reg_f3d<PRECISION_TYPE> *reg = new reg_f3d<PRECISION_TYPE>(targetImage->nt, sourceImage->nt);
        
        // Create a vector to hold the number of completed iterations
        int *completedIterations = (int *) calloc(nLevels, sizeof(int));

#ifdef _OPENMP
        int maxThreadNumber = omp_get_max_threads();
        if (verbose)
            Rprintf("[NiftyReg F3D] OpenMP is used with %i thread(s)\n", maxThreadNumber);
#endif

        // Set the reg_f3d parameters
        reg->SetReferenceImage(targetImage);
        reg->SetFloatingImage(sourceImage);

        if (verbose)
            reg->PrintOutInformation();
        else
            reg->DoNotPrintOutInformation();

        if (targetMaskImage != NULL)
           reg->SetReferenceMask(targetMaskImage);

        if (controlPointImage != NULL)
           reg->SetControlPointGridImage(controlPointImage);

        if (affineTransformation != NULL)
           reg->SetAffineTransformation(affineTransformation);

        reg->SetBendingEnergyWeight(bendingEnergyWeight);

        // if (linearEnergyWeight0==linearEnergyWeight0 || linearEnergyWeight1==linearEnergyWeight1 || linearEnergyWeight2==linearEnergyWeight2)
        // {
        //     if(linearEnergyWeight0!=linearEnergyWeight0) linearEnergyWeight0=0.0;
        //     if(linearEnergyWeight1!=linearEnergyWeight1) linearEnergyWeight1=0.0;
        //     if(linearEnergyWeight2!=linearEnergyWeight2) linearEnergyWeight2=0.0;
        //     reg->SetLinearEnergyWeights(linearEnergyWeight0,linearEnergyWeight1,linearEnergyWeight2);
        // }
    
        reg->SetJacobianLogWeight(jacobianWeight);

        reg->SetMaximalIterationNumber(maxIterations);

        if (nBins > 0)
        {
            for (int i = 0; i < targetImage->nt; i++)
            {
                reg->SetReferenceBinNumber(i, (unsigned) nBins);
                reg->SetFloatingBinNumber(i, (unsigned) nBins);
            }
        }

        for (int i = 0; i < 3; i++)
            reg->SetSpacing((unsigned) i, (PRECISION_TYPE) spacing[i]);

        reg->SetLevelNumber(nLevels);
        reg->SetLevelToPerform(nLevels);

        if (finalInterpolation == 3)
            reg->UseCubicSplineInterpolation();
        else if (finalInterpolation == 1)
            reg->UseLinearInterpolation();
        else
            reg->UseNeareatNeighborInterpolation();
    
        // Run the registration
        reg->Run_f3d();
    
        memcpy(completedIterations, reg->GetCompletedIterations(), nLevels*sizeof(int));

        result.image = reg->GetWarpedImage();
        result.controlPoints = reg->GetControlPointPositionImage();
        result.completedIterations = completedIterations;
    
        // Erase the registration object
        delete reg;
    }
    
    return result;
}
