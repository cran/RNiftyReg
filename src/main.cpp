#include <RcppEigen.h>

#include "config.h"
#include "NiftiImage.h"
#include "DeformationField.h"
#include "aladin.h"
#include "f3d.h"
#include "_reg_nmi.h"

// Registration types (degrees of freedom)
#define TYPE_RIGID  0
#define TYPE_AFFINE 1

using namespace Rcpp;

typedef std::vector<float> float_vector;

std::map<std::string,short> NiftiImage::DatatypeCodes = NiftiImage::buildDatatypeCodes();

RcppExport SEXP retrieveImage (SEXP _image)
{
BEGIN_RCPP
    NiftiImage image(_image);
    return image.toPointer("NIfTI image");
END_RCPP
}

RcppExport SEXP readNifti (SEXP _file, SEXP _internal)
{
BEGIN_RCPP
    NiftiImage image(_file);
    return image.toArrayOrPointer(as<bool>(_internal), "NIfTI image");
END_RCPP
}

RcppExport SEXP writeNifti (SEXP _image, SEXP _file, SEXP _datatype)
{
BEGIN_RCPP
    NiftiImage image(_image);
    std::string datatypeString = as<std::string>(_datatype);
    if (NiftiImage::DatatypeCodes.count(datatypeString) == 0)
    {
        std::ostringstream message;
        message << "Datatype \"" << datatypeString << "\" is not valid";
        Rf_warning(message.str().c_str());
        
        datatypeString = "auto";
    }
    image.toFile(as<std::string>(_file), NiftiImage::DatatypeCodes[datatypeString]);
    return R_NilValue;
END_RCPP
}

RcppExport SEXP updateNifti (SEXP _image, SEXP _reference)
{
BEGIN_RCPP
    const NiftiImage reference(_reference);
    RObject image(_image);
    
    if (!reference.isNull())
    {
        NiftiImage updatedImage = reference;
        updatedImage.update(image);
        return updatedImage.toArray();
    }
    else
        return image;
END_RCPP
}

RcppExport SEXP dumpNifti (SEXP _image)
{
BEGIN_RCPP
    const NiftiImage image(_image, false);
    return image.headerToList();
END_RCPP
}

RcppExport SEXP getXform (SEXP _image, SEXP _preferQuaternion)
{
BEGIN_RCPP
    const NiftiImage image(_image, false);
    const bool preferQuaternion = as<bool>(_preferQuaternion);
    
    AffineMatrix matrix(image.xform(preferQuaternion), false);
    return matrix;
END_RCPP
}

RcppExport SEXP setXform (SEXP _image, SEXP _matrix, SEXP _isQform)
{
BEGIN_RCPP
    NiftiImage image(_image);
    AffineMatrix matrix(_matrix);
    
    int code = -1;
    if (!Rf_isNull(matrix.attr("code")))
        code = as<int>(matrix.attr("code"));
    
    if (!image.isNull())
    {
        if (as<bool>(_isQform))
        {
            image->qto_xyz = mat44(matrix);
            image->qto_ijk = nifti_mat44_inverse(image->qto_xyz);
            nifti_mat44_to_quatern(image->qto_xyz, &image->quatern_b, &image->quatern_c, &image->quatern_d, &image->qoffset_x, &image->qoffset_y, &image->qoffset_z, NULL, NULL, NULL, &image->qfac);
            
            if (code >= 0)
                image->qform_code = code;
        }
        else
        {
            image->sto_xyz = mat44(matrix);
            image->sto_ijk = nifti_mat44_inverse(image->sto_xyz);
            
            if (code >= 0)
                image->sform_code = code;
        }
    }
    
    if (image.isPersistent())
        return _image;
    else
        return image.toArray();
END_RCPP
}

RcppExport SEXP rescaleImage (SEXP _image, SEXP _scales)
{
BEGIN_RCPP
    const std::vector<float> scales = as<float_vector>(_scales);
    const NiftiImage image(_image);
    
    NiftiImage newImage(nifti_copy_nim_info(image));
    newImage.rescale(scales);
    return newImage.toPointer("NIfTI image");
END_RCPP
}

void checkImages (const NiftiImage &sourceImage, const NiftiImage &targetImage)
{
    if (sourceImage.isNull())
        throw std::runtime_error("Cannot read or retrieve source image");
    if (targetImage.isNull())
        throw std::runtime_error("Cannot read or retrieve target image");
    
    const int nSourceDim = sourceImage.nDims();
    const int nTargetDim = targetImage.nDims();
    
    if (nSourceDim < 2 || nSourceDim > 4)
        throw std::runtime_error("Source image should have 2, 3 or 4 dimensions");
    if (nTargetDim < 2 || nTargetDim > 3)
        throw std::runtime_error("Target image should have 2 or 3 dimensions");
}

RcppExport SEXP calculateMeasure (SEXP _source, SEXP _target, SEXP _targetMask, SEXP _interpolation)
{
BEGIN_RCPP
    NiftiImage sourceImage(_source);
    NiftiImage targetImage(_target);
    NiftiImage targetMask(_targetMask);
    
    checkImages(sourceImage.drop(), targetImage.drop());
    if (sourceImage.nDims() != targetImage.nDims())
        throw std::runtime_error("Images should have the same dimensionality");
    
    int *targetMaskData = NULL;
    int targetVoxelCount3D = targetImage->nx * targetImage->ny * targetImage->nz;
    if (targetMask.isNull())
    {
        targetMaskData = (int *) calloc(targetVoxelCount3D, sizeof(int));
        for (int i=0; i<targetVoxelCount3D; i++)
            targetMaskData[i] = i;
    }
    else
    {
        reg_checkAndCorrectDimension(targetMask);
        reg_createMaskPyramid<PRECISION_TYPE>(targetMask, &targetMaskData, 1, 1, &targetVoxelCount3D);
    }
    
    reg_tools_changeDatatype<PRECISION_TYPE>(sourceImage);
    reg_tools_changeDatatype<PRECISION_TYPE>(targetImage);
    
    AffineMatrix affine(sourceImage, targetImage);
    DeformationField deformationField(targetImage, affine);
    NiftiImage resampledSourceImage = deformationField.resampleImage(sourceImage, as<int>(_interpolation));
    
    reg_nmi nmi;
    for (int i=0; i<std::min(targetImage->nt,resampledSourceImage->nt); i++)
        nmi.SetActiveTimepoint(i);
    nmi.InitialiseMeasure(targetImage, resampledSourceImage, targetMaskData, resampledSourceImage, NULL, NULL);
    double measure = nmi.GetSimilarityMeasureValue();
    
    free(targetMaskData);
    
    return wrap(measure);
END_RCPP
}

RcppExport SEXP regLinear (SEXP _source, SEXP _target, SEXP _type, SEXP _symmetric, SEXP _nLevels, SEXP _maxIterations, SEXP _useBlockPercentage, SEXP _interpolation, SEXP _sourceMask, SEXP _targetMask, SEXP _init, SEXP _verbose, SEXP _estimateOnly, SEXP _sequentialInit, SEXP _internal)
{
BEGIN_RCPP
    NiftiImage sourceImage(_source);
    NiftiImage targetImage(_target);
    NiftiImage sourceMask(_sourceMask);
    NiftiImage targetMask(_targetMask);
    
    checkImages(sourceImage.drop(), targetImage.drop());
    
    const LinearTransformScope scope = (as<int>(_type) == TYPE_AFFINE ? AffineScope : RigidScope);
    const int interpolation = as<int>(_interpolation);
    const bool symmetric = as<bool>(_symmetric);
    const bool estimateOnly = as<bool>(_estimateOnly);
    const bool sequentialInit = as<bool>(_sequentialInit);
    
    List init(_init);
    List returnValue;
    
    if (sourceImage.nDims() == targetImage.nDims())
    {
        AffineMatrix initAffine;
        if (!Rf_isNull(init[0]))
            initAffine = AffineMatrix(SEXP(init[0]));
        else
            initAffine = AffineMatrix(sourceImage, targetImage);
    
        AladinResult result = regAladin(sourceImage, targetImage, scope, symmetric, as<int>(_nLevels), as<int>(_maxIterations), as<int>(_useBlockPercentage), as<int>(_interpolation), sourceMask, targetMask, initAffine, as<bool>(_verbose), estimateOnly);
        
        returnValue["image"] = result.image.toArrayOrPointer(as<bool>(_internal), "Result image");
        returnValue["forwardTransforms"] = List::create(result.forwardTransform);
        if (symmetric)
            returnValue["reverseTransforms"] = List::create(result.reverseTransform);
        else
            returnValue["reverseTransforms"] = R_NilValue;
        returnValue["iterations"] = List::create(result.iterations);
        returnValue["source"] = List::create(sourceImage.toPointer("Source image"));
        returnValue["target"] = targetImage.toPointer("Target image");
        
        return returnValue;
    }
    else if (sourceImage.nDims() - targetImage.nDims() == 1)
    {
        const int nReps = sourceImage->dim[sourceImage.nDims()];
        List forwardTransforms(nReps), reverseTransforms(nReps), iterations(nReps), sourceImages(nReps);
        NiftiImage finalImage = allocateMultiregResult(sourceImage, targetImage, interpolation != 0);
        AladinResult result;
        for (int i=0; i<nReps; i++)
        {
            NiftiImage currentSource;
            if (sourceImage.nDims() == 3)
                currentSource = sourceImage.slice(i);
            else
                currentSource = sourceImage.volume(i);
            sourceImages[i] = currentSource.toPointer("Source image");
            
            AffineMatrix initAffine;
            if (!Rf_isNull(init[i]))
                initAffine = AffineMatrix(SEXP(init[i]));
            else if (sequentialInit && i>0 && result.forwardTransform.isValid())
                initAffine = result.forwardTransform;
            else
                initAffine = AffineMatrix(currentSource, targetImage);
            
            result = regAladin(currentSource, targetImage, scope, symmetric, as<int>(_nLevels), as<int>(_maxIterations), as<int>(_useBlockPercentage), interpolation, sourceMask, targetMask, initAffine, as<bool>(_verbose), estimateOnly);
            
            if (sourceImage.nDims() == 3)
                finalImage.slice(i) = result.image;
            else
                finalImage.volume(i) = result.image;
            
            forwardTransforms[i] = result.forwardTransform;
            if (symmetric)
                reverseTransforms[i] = result.reverseTransform;
            iterations[i] = result.iterations;
        }
        
        returnValue["image"] = finalImage.toArrayOrPointer(as<bool>(_internal), "Result image");
        returnValue["forwardTransforms"] = forwardTransforms;
        if (symmetric)
            returnValue["reverseTransforms"] = reverseTransforms;
        else
            returnValue["reverseTransforms"] = R_NilValue;
        returnValue["iterations"] = iterations;
        returnValue["source"] = sourceImages;
        returnValue["target"] = targetImage.toPointer("Target image");
        
        return returnValue;
    }
    else
    {
        std::ostringstream message;
        message << "Cannot register a " << sourceImage.nDims() << "D source image to a " << targetImage.nDims() << "D target";
        throw std::runtime_error(message.str());
    }
    
    return R_NilValue;
END_RCPP
}

RcppExport SEXP regNonlinear (SEXP _source, SEXP _target, SEXP _symmetric, SEXP _nLevels, SEXP _maxIterations, SEXP _interpolation, SEXP _sourceMask, SEXP _targetMask, SEXP _init, SEXP _nBins, SEXP _spacing, SEXP _bendingEnergyWeight, SEXP _linearEnergyWeight, SEXP _jacobianWeight, SEXP _verbose, SEXP _estimateOnly, SEXP _sequentialInit, SEXP _internal)
{
BEGIN_RCPP
    NiftiImage sourceImage(_source);
    NiftiImage targetImage(_target);
    NiftiImage sourceMask(_sourceMask);
    NiftiImage targetMask(_targetMask);
    
    checkImages(sourceImage.drop(), targetImage.drop());
    
    const int interpolation = as<int>(_interpolation);
    const bool symmetric = as<bool>(_symmetric);
    const bool estimateOnly = as<bool>(_estimateOnly);
    const bool sequentialInit = as<bool>(_sequentialInit);
    
    List init(_init);
    List returnValue;
    
    if (sourceImage.nDims() == targetImage.nDims())
    {
        AffineMatrix initAffine;
        NiftiImage initControl;
        if (!Rf_isNull(init[0]))
        {
            // NB: R code must set the class of an affine appropriately
            RObject initObject(init[0]);
            if (initObject.inherits("affine"))
                initAffine = AffineMatrix(SEXP(initObject));
            else
                initControl = NiftiImage(SEXP(init[0]));
        }
        else
            initAffine = AffineMatrix(sourceImage, targetImage);
    
        F3dResult result = regF3d(sourceImage, targetImage, as<int>(_nLevels), as<int>(_maxIterations), interpolation, sourceMask, targetMask, initControl, initAffine, as<int>(_nBins), as<float_vector>(_spacing), as<float>(_bendingEnergyWeight), as<float>(_linearEnergyWeight), as<float>(_jacobianWeight), symmetric, as<bool>(_verbose), estimateOnly);
        
        returnValue["image"] = result.image.toArrayOrPointer(as<bool>(_internal), "Result image");
        returnValue["forwardTransforms"] = List::create(result.forwardTransform.toPointer("F3D control points"));
        if (symmetric)
            returnValue["reverseTransforms"] = List::create(result.reverseTransform.toPointer("F3D control points"));
        else
            returnValue["reverseTransforms"] = R_NilValue;
        returnValue["iterations"] = List::create(result.iterations);
        returnValue["source"] = List::create(sourceImage.toPointer("Source image"));
        returnValue["target"] = targetImage.toPointer("Target image");
        
        return returnValue;
    }
    else if (sourceImage.nDims() - targetImage.nDims() == 1)
    {
        const int nReps = sourceImage->dim[sourceImage.nDims()];
        List forwardTransforms(nReps), reverseTransforms(nReps), iterations(nReps), sourceImages(nReps);
        NiftiImage finalImage = allocateMultiregResult(sourceImage, targetImage, interpolation != 0);
        F3dResult result;
        for (int i=0; i<nReps; i++)
        {
            NiftiImage currentSource;
            if (sourceImage.nDims() == 3)
                currentSource = sourceImage.slice(i);
            else
                currentSource = sourceImage.volume(i);
            sourceImages[i] = currentSource.toPointer("Source image");
            
            AffineMatrix initAffine;
            NiftiImage initControl;
            if (!Rf_isNull(init[i]))
            {
                // NB: R code must set the class of an affine appropriately
                RObject initObject(init[i]);
                if (initObject.inherits("affine"))
                    initAffine = AffineMatrix(SEXP(initObject));
                else
                    initControl = NiftiImage(SEXP(init[i]));
            }
            else if (sequentialInit && i>0 && !result.forwardTransform.isNull())
                initControl = result.forwardTransform;
            else
                initAffine = AffineMatrix(currentSource, targetImage);
            
            result = regF3d(currentSource, targetImage, as<int>(_nLevels), as<int>(_maxIterations), interpolation, sourceMask, targetMask, initControl, initAffine, as<int>(_nBins), as<float_vector>(_spacing), as<float>(_bendingEnergyWeight), as<float>(_linearEnergyWeight), as<float>(_jacobianWeight), symmetric, as<bool>(_verbose), estimateOnly);
            
            if (sourceImage.nDims() == 3)
                finalImage.slice(i) = result.image;
            else
                finalImage.volume(i) = result.image;
            
            forwardTransforms[i] = result.forwardTransform.toPointer("F3D control points");
            if (symmetric)
                reverseTransforms[i] = result.reverseTransform.toPointer("F3D control points");
            iterations[i] = result.iterations;
        }
        
        returnValue["image"] = finalImage.toArrayOrPointer(as<bool>(_internal), "Result image");
        returnValue["forwardTransforms"] = forwardTransforms;
        if (symmetric)
            returnValue["reverseTransforms"] = reverseTransforms;
        else
            returnValue["reverseTransforms"] = R_NilValue;
        returnValue["iterations"] = iterations;
        returnValue["source"] = sourceImages;
        returnValue["target"] = targetImage.toPointer("Target image");
        
        return returnValue;
    }
    else
    {
        std::ostringstream message;
        message << "Cannot register a " << sourceImage.nDims() << "D source image to a " << targetImage.nDims() << "D target";
        throw std::runtime_error(message.str());
    }
    
    return R_NilValue;
END_RCPP
}

RcppExport SEXP getDeformationField (SEXP _transform, SEXP _jacobian)
{
BEGIN_RCPP
    RObject transform(_transform);
    RObject result;
    NiftiImage targetImage(SEXP(transform.attr("target")));
    DeformationField field;
    
    if (transform.inherits("affine"))
    {
        AffineMatrix affine = AffineMatrix(SEXP(transform));
        field = DeformationField(targetImage, affine);
    }
    else
    {
        NiftiImage transformationImage(_transform);
        field = DeformationField(targetImage, transformationImage);
    }
    
    result = field.getFieldImage().toPointer("Deformation field");
    result.attr("source") = transform.attr("source");
    result.attr("target") = transform.attr("target");
    
    if (as<bool>(_jacobian))
        result.attr("jacobian") = field.getJacobian().toPointer("Jacobian of deformation field");
    
    return result;
END_RCPP
}

RcppExport SEXP transformPoints (SEXP _transform, SEXP _points, SEXP _nearest)
{
BEGIN_RCPP
    NiftiImage transformationImage(_transform);
    RObject transform(_transform);
    NiftiImage sourceImage(SEXP(transform.attr("source")), false);
    NiftiImage targetImage(SEXP(transform.attr("target")), false);
    DeformationField deformationField(targetImage, transformationImage);
    NumericMatrix points(_points);
    List result(points.nrow());
    const bool nearest = as<bool>(_nearest);
    
    if (points.ncol() == 2)
    {
        for (int i=0; i<points.nrow(); i++)
        {
            Eigen::Vector2d point;
            point[0] = points(i, 0);
            point[1] = points(i, 1);
            result[i] = deformationField.findPoint(sourceImage, point, nearest);
        }
    }
    else if (points.ncol() == 3)
    {
        for (int i=0; i<points.nrow(); i++)
        {
            Eigen::Vector3d point;
            point[0] = points(i, 0);
            point[1] = points(i, 1);
            point[2] = points(i, 2);
            result[i] = deformationField.findPoint(sourceImage, point, nearest);
        }
    }
    else
        throw std::runtime_error("Points matrix should have 2 or 3 columns");
    
    return result;
END_RCPP
}

RcppExport SEXP pointerToArray (SEXP _image)
{
BEGIN_RCPP
    NiftiImage image(_image);
    return image.toArray();
END_RCPP
}

RcppExport SEXP halfTransform (SEXP _transform)
{
BEGIN_RCPP
    RObject transform(_transform);
    RObject result;
    if (transform.inherits("affine"))
    {
        Eigen::MatrixXd matrix = as<Eigen::MatrixXd>(_transform);
        matrix = (matrix.log() * 0.5).exp();
        result = AffineMatrix(matrix);
    }
    else
    {
        NiftiImage transformationImage(_transform);
        switch (reg_round(transformationImage->intent_p1))
        {
            case SPLINE_GRID:
            reg_getDisplacementFromDeformation(transformationImage);
            reg_tools_multiplyValueToImage(transformationImage,transformationImage,0.5f);
            reg_getDeformationFromDisplacement(transformationImage);
            break;
            
            case DEF_FIELD:
            reg_getDisplacementFromDeformation(transformationImage);
            reg_tools_multiplyValueToImage(transformationImage,transformationImage,0.5f);
            reg_getDeformationFromDisplacement(transformationImage);
            break;
            
            case DISP_FIELD:
            reg_tools_multiplyValueToImage(transformationImage,transformationImage,0.5f);
            break;
            
            case SPLINE_VEL_GRID:
            reg_getDisplacementFromDeformation(transformationImage);
            reg_tools_multiplyValueToImage(transformationImage,transformationImage,0.5f);
            reg_getDeformationFromDisplacement(transformationImage);
            --transformationImage->intent_p2;
            if (transformationImage->num_ext>1)
                --transformationImage->num_ext;
            break;
            
            case DEF_VEL_FIELD:
            reg_getDisplacementFromDeformation(transformationImage);
            reg_tools_multiplyValueToImage(transformationImage,transformationImage,0.5f);
            reg_getDeformationFromDisplacement(transformationImage);
            --transformationImage->intent_p2;
            break;
            
            case DISP_VEL_FIELD:
            reg_tools_multiplyValueToImage(transformationImage,transformationImage,0.5f);
            --transformationImage->intent_p2;
            break;
            
            default:
            throw std::runtime_error("The specified transformation image is not valid or not supported");
        }
        
        result = transformationImage.toPointer("F3D transformation");
    }
    
    result.attr("source") = transform.attr("source");
    result.attr("target") = transform.attr("target");
    
    return result;
END_RCPP
}

RcppExport SEXP composeTransforms (SEXP _transform1, SEXP _transform2)
{
BEGIN_RCPP
    RObject transform1(_transform1);
    RObject transform2(_transform2);
    RObject result;
    
    if (Rf_isNull(_transform1))
        return _transform2;
    else if (Rf_isNull(_transform2))
        return _transform1;
    else if (transform1.inherits("affine") && transform2.inherits("affine"))
    {
        Eigen::MatrixXd matrix = as<Eigen::MatrixXd>(_transform2) * as<Eigen::MatrixXd>(_transform1);
        result = AffineMatrix(matrix);
    }
    else
    {
        DeformationField field1, field2;
        
        NiftiImage targetImage1(SEXP(transform1.attr("target")));
        NiftiImage targetImage2(SEXP(transform2.attr("target")));
        
        if (transform1.inherits("affine"))
        {
            AffineMatrix transformMatrix(_transform1);
            field1 = DeformationField(targetImage1, transformMatrix, true);
        }
        else
        {
            NiftiImage transformImage(_transform1);
            field1 = DeformationField(targetImage1, transformImage, true);
        }
        
        if (transform2.inherits("affine"))
        {
            AffineMatrix transformMatrix(_transform2);
            field2 = DeformationField(targetImage2, transformMatrix, true);
        }
        else
        {
            NiftiImage transformImage(_transform2);
            field2 = DeformationField(targetImage2, transformImage, true);
        }
        
        // Order of composition is possibly not as expected
        field2.compose(field1);
        result = field2.getFieldImage().toPointer("Deformation field");
    }
    
    result.attr("source") = transform1.attr("source");
    result.attr("target") = transform2.attr("target");
    
    return result;
END_RCPP
}
