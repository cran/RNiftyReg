#' Create, test for and print affine matrices
#' 
#' \code{isAffine} returns a logical value indicating whether its argument is,
#' or resembles, a 4x4 affine matrix. \code{asAffine} converts other objects to
#' the affine class, attaching or updating the source and target image
#' attributes. Affine transformations are a class of linear transformations
#' which preserve points, straight lines and planes, and may consist of a
#' combination of rotation, translation, scale and skew operations.
#' 
#' NiftyReg's convention is for affine matrices to transform world coordinates
#' (in the sense of \code{voxelToWorld}) from TARGET to SOURCE space, although
#' transforms are logically applied the other way.
#' 
#' @param object An R object.
#' @param strict If \code{TRUE}, this function just tests whether the object is
#'   of class \code{"affine"}. Otherwise it also tests for an affine-like 4x4
#'   matrix.
#' @param source,target New source and target images for the transformation.
#' @param x An \code{"affine"} object.
#' @param ... Additional parameters to methods.
#' @param i The transformation number, for \code{niftyreg} objects containing
#'   more than one.
#' @return For \code{isAffine}, a logical value, which is \code{TRUE} if
#'   \code{object} appears to be an affine matrix. For \code{asAffine}, a
#'   classed affine object with source and target attributes set appropriately.
#' 
#' @note 2D affines are a subset of 3D affines, and are stored in a 4x4 matrix
#'   for internal consistency, even though a 3x3 matrix would suffice.
#' @author Jon Clayden <code@@clayden.org>
#' @aliases affine
#' @rdname affine
#' @export
isAffine <- function (object, strict = FALSE)
{
    if (inherits(object, "affine"))
        return (TRUE)
    else if (!strict && is.matrix(object) && isTRUE(all.equal(dim(object), c(4,4))))
        return (TRUE)
    else
        return (FALSE)
}


#' @rdname affine
#' @export
asAffine <- function (object, source = NULL, target = NULL, ...)
{
    UseMethod("asAffine")
}

#' @rdname affine
#' @method asAffine niftyreg
#' @export
asAffine.niftyreg <- function (object, source = NULL, target = NULL, i = 1L, ...)
{
    asAffine(forward(object, i), source, target, ...)
}

#' @rdname affine
#' @export
asAffine.niftyregRDS <- function (object, source = NULL, target = NULL, ...)
{
    asAffine(loadTransform(object), source, target, ...)
}

#' @rdname affine
#' @export
asAffine.affine <- function (object, source = NULL, target = NULL, ...)
{
    return (xfmAttrib(object, source, target, ...))
}

#' @rdname affine
#' @export
asAffine.niftiImage <- function (object, source = attr(object,"source"), target = attr(object,"target"), ...)
{
    # For some reason, NiftyReg stores the half transform twice
    halfTransforms <- lapply(extensions(object), function(ext) {
        values <- readBin(ext, "numeric", 16L, 4L)
        return (matrix(values, nrow=4L, ncol=4L, byrow=TRUE))
    })
    
    if (length(halfTransforms) != 2L)
        stop("NiftyReg embedded affine is not present, or in an unexpected form")
    else if (object$intent_name != "NREG_TRANS")
        warning("This image doesn't look like a NiftyReg transform; results may be misleading")
    
    transform <- do.call("%*%", halfTransforms)
    return (xfmAttrib(transform, source, target, class="affine"))
}

#' @rdname affine
#' @export
asAffine.default <- function (object, source = NULL, target = NULL, ...)
{
    object <- as.matrix(object)
    
    if (!isTRUE(all.equal(dim(object), c(4,4))))
        stop("Affine matrix should be 4x4")
    
    return (xfmAttrib(object, source, target, class="affine"))
}


#' @rdname affine
#' @export
print.affine <- function (x, ...)
{
    cat("NiftyReg affine matrix:\n")
    lines <- apply(format(round(x,6),scientific=FALSE), 1, paste, collapse="  ")
    cat(paste0("  ",lines), sep="\n")
    
    source <- attr(x, "source")
    if (!is.null(source))
        cat(paste0("\nSource origin: (", paste(round(worldToVoxel(c(0,0,0),source),2),collapse=", "), ")"))
    
    target <- attr(x, "target")
    if (!is.null(target))
        cat(paste0("\nTarget origin: (", paste(round(worldToVoxel(c(0,0,0),target),2),collapse=", "), ")"))
    
    cat("\n")
}


#' Read an affine matrix from a file
#' 
#' This function is used to read a 4x4 numeric matrix representing an affine
#' transformation from a file. It is a wrapper around \code{read.table} which
#' additionally ensures that required attributes are set. The type of the
#' matrix must be specified, as there are differing conventions across
#' software packages.
#' 
#' @param fileName A string giving the file name to read the affine matrix
#'   from.
#' @param source The source image for the transformation. If \code{NULL}, the
#'   file will be searched for a comment specifying the path to a NIfTI file.
#' @param target The target image for the transformation. If \code{NULL}, the
#'   file will be searched for a comment specifying the path to a NIfTI file.
#' @param type The type of the affine matrix, which describes what convention
#'   is it is stored with. Currently valid values are \code{"niftyreg"} and
#'   \code{"fsl"} (for FSL FLIRT). If \code{NULL}, the function will look in
#'   the file for a comment specifying the type.
#' @return An matrix with class \code{"affine"}, converted to the NiftyReg
#'   convention and with \code{source} and \code{target} attributes set
#'   appropriately.
#' 
#' @examples
#' print(readAffine(system.file("extdata","affine.txt",package="RNiftyReg")))
#' 
#' @author Jon Clayden <code@@clayden.org>
#' @seealso \code{\link{read.table}}, \code{\link{writeAffine}}
#' @export
readAffine <- function (fileName, source = NULL, target = NULL, type = NULL)
{
    if (!is.null(type))
        type <- match.arg(tolower(type), c("niftyreg","fsl"))
    
    lines <- readLines(fileName)
    
    match <- regexpr("^\\s*# source:\\s*(.+)$", lines, perl=TRUE)
    if (is.null(source) && any(match > 0))
    {
        index <- which(match > 0)[1]
        source <- substr(lines[index], attr(match,"capture.start")[index], attr(match,"capture.start")[index]+attr(match,"capture.length")[index])
    }
    match <- regexpr("^\\s*# target:\\s*(.+)$", lines, perl=TRUE)
    if (is.null(target) && any(match > 0))
    {
        index <- which(match > 0)[1]
        target <- substr(lines[index], attr(match,"capture.start")[index], attr(match,"capture.start")[index]+attr(match,"capture.length")[index])
    }
    match <- regexpr("^\\s*# affineType:\\s*(\\w+)\\s*$", lines, perl=TRUE)
    if (is.null(type))
    {
        if (any(match > 0))
        {
            index <- which(match > 0)[1]
            type <- substr(lines[index], attr(match,"capture.start")[index], attr(match,"capture.start")[index]+attr(match,"capture.length")[index])
            type <- match.arg(tolower(type), c("niftyreg","fsl"))
        }
        else
            type <- "niftyreg"
    }
    
    connection <- textConnection(grep("^\\s*#", lines, perl=TRUE, value=TRUE, invert=TRUE))
    affine <- as.matrix(read.table(connection))
    close(connection)
    
    if (!isTRUE(all.equal(dim(affine), c(4,4))))
        stop("The specified file does not contain a 4x4 affine matrix")
    
    affine <- asAffine(affine, source, target)
    if (type != "niftyreg")
        affine <- convertAffine(affine, source, target, "niftyreg")
    
    return (affine)
}


#' Write an affine matrix to a file
#' 
#' This function is used to write a 4x4 numeric matrix representing an affine
#' transformation to a file. A comment is also (optionally) written, which
#' specifies the matrix as using the NiftyReg convention, for the benefit of
#' \code{\link{readAffine}}.
#' 
#' @param affine A 4x4 affine matrix.
#' @param fileName A string giving the file name to write the matrix to.
#' @param comments Logical value: if \code{TRUE} comments are written to the
#'   file in lines beginning with \code{#}.
#' 
#' @author Jon Clayden <code@@clayden.org>
#' @seealso \code{\link{write.table}}, \code{\link{readAffine}}
#' @export
writeAffine <- function (affine, fileName, comments = TRUE)
{
    if (!isAffine(affine))
        stop("Specified affine matrix is not valid")
    
    lines <- apply(format(affine,scientific=FALSE), 1, paste, collapse="  ")
    if (comments)
        lines <- c("# affineType: niftyreg", lines)
    writeLines(lines, fileName)
}


# For internal use only: users should call applyTransform()
applyAffine <- function (affine, points)
{
    if (!isAffine(affine))
        stop("Specified affine matrix is not valid")
    
    if (!is.matrix(points))
        points <- matrix(points, nrow=1)
    
    nDims <- ncol(points)
    if (nDims != 2 && nDims != 3)
        stop("Points must be two or three dimensional")
    
    if (nDims == 2)
        affine <- matrix(affine[c(1,2,4,5,6,8,13,14,16)], ncol=3, nrow=3)
    
    points <- cbind(points, 1)
    newPoints <- affine %*% t(points)
    newPoints <- drop(t(newPoints[1:nDims,,drop=FALSE]))
    
    return (newPoints)
}


# For internal use only: working transforms should always use the NiftyReg convention
convertAffine <- function (affine, source = NULL, target = NULL, newType = c("niftyreg","fsl"))
{
    if (!isAffine(affine))
        stop("Specified affine matrix is not valid")
    if (is.null(source))
        source <- attr(affine, "source")
    if (is.null(target))
        target <- attr(affine, "target")
    
    newType <- match.arg(newType)
    
    sourceXform <- xform(source, useQuaternionFirst=FALSE)
    targetXform <- xform(target, useQuaternionFirst=FALSE)
    sourceScaling <- diag(c(sqrt(colSums(sourceXform[1:3,1:3]^2)), 1))
    targetScaling <- diag(c(sqrt(colSums(targetXform[1:3,1:3]^2)), 1))
    
    # Handle FSL/FLIRT quirk where images whose xforms have positive determinant are flipped before transformation
    if (det(sourceXform[1:3,1:3]) > 0)
    {
        orient <- unlist(strsplit(orientation(sourceXform), ""))
        orient[1] <- switch(orient[1], R="L", L="R", A="P", P="A", S="I", I="S")
        orientation(sourceXform) <- paste(orient, collapse="")
    }
    if (det(targetXform[1:3,1:3]) > 0)
    {
        orient <- unlist(strsplit(orientation(targetXform), ""))
        orient[1] <- switch(orient[1], R="L", L="R", A="P", P="A", S="I", I="S")
        orientation(targetXform) <- paste(orient, collapse="")
    }
    
    # NiftyReg transforms convert world coordinates from target to source space
    # FSL transforms convert pseudo-world coordinates (scaled only) from source to target space
    if (newType == "fsl")
        newAffine <- targetScaling %*% solve(targetXform) %*% solve(affine) %*% sourceXform %*% solve(sourceScaling)
    else
        newAffine <- sourceXform %*% solve(sourceScaling) %*% solve(affine) %*% targetScaling %*% solve(targetXform)
    
    return (asAffine(newAffine, source, target))
}


#' Invert an affine matrix
#' 
#' This function is used to invert an affine matrix. It is a wrapper around
#' \code{\link{solve}}, which additionally sets appropriate attributes.
#' 
#' @param affine An existing 4x4 affine matrix.
#' @return The inverted affine matrix.
#' 
#' @examples
#' affine <- readAffine(system.file("extdata","affine.txt",package="RNiftyReg"))
#' print(affine)
#' print(invertAffine(affine))
#' 
#' @author Jon Clayden <code@@clayden.org>
#' @seealso \code{\link{solve}}
#' @export
invertAffine <- function (affine)
{
    if (!isAffine(affine))
        stop("Specified affine matrix is not valid")
    
    newAffine <- solve(affine)
    return (asAffine(newAffine, attr(affine,"target"), attr(affine,"source")))
}


#' Build an affine matrix up from its constituent transformations
#' 
#' This function does the opposite to \code{\link{decomposeAffine}}, building
#' up an affine matrix from its components. It can be useful for testing, or
#' for rescaling images.
#' 
#' @param translation Translations along each axis, in \code{\link{pixunits}}
#'   units. May also be a list, such as that produced by
#'   \code{\link{decomposeAffine}}, with elements for translation, scales,
#'   skews and angles.
#' @param scales Scale factors along each axis.
#' @param skews Skews in the XY, XZ and YZ planes.
#' @param angles Roll, pitch and yaw rotation angles, in radians. If
#'   \code{source} is two-dimensional, a single angle will be interpreted as
#'   being in the plane as expected.
#' @param source The source image for the transformation (required).
#' @param target The target image for the transformation. If \code{NULL} (the
#'   default), it will be equal to \code{source}, or a rescaled version of it
#'   if any of the \code{scales} are not 1. In the latter case the scales will
#'   be reset back to 1 to produce the right effect.
#' @param anchor The fixed point for the transformation. Setting this parameter
#'   to a value other than \code{"none"} will override the \code{translation}
#'   parameter, with the final translation set to ensure that the requested
#'   point remains in the same place after transformation.
#' @return A 4x4 affine matrix representing the composite transformation. Note
#'   that NiftyReg affines logically transform backwards, from target to source
#'   space, so the matrix may be the inverse of what is expected.
#' 
#' @author Jon Clayden <code@@clayden.org>
#' @seealso \code{\link{decomposeAffine}}, \code{\link{isAffine}}
#' @export
buildAffine <- function (translation = c(0,0,0), scales = c(1,1,1), skews = c(0,0,0), angles = c(0,0,0), source = NULL, target = NULL, anchor = c("none","origin","centre","center"))
{
    if (is.null(source))
        stop("Source image must be specified")
    source <- asNifti(source, internal=TRUE)
    
    if (is.list(translation))
        x <- translation
    else
        x <- list(translation=translation, scales=scales, skews=skews, angles=angles)
    
    if (any(x$scales == 0))
        stop("Scales should not be zero")
    if (length(x$scales) < 3)
        x$scales <- c(x$scales, rep(1,3-length(x$scales)))
    if (length(x$angles) == 1 && (ndim(source) == 2 || (ndim(source) == 3 && dim(source)[3] <= 4)))
        x$angles <- c(0, 0, x$angles)
    for (name in c("translation","skews","angles"))
    {
        if (length(x[[name]]) < 3)
            x[[name]] <- c(x[[name]], rep(0,3-length(x[[name]])))
    }
    
    if (is.null(target))
    {
        if (all(abs(x$scales) == 1))
            target <- source
        else
        {
            target <- RNifti:::rescaleNifti(source, abs(x$scales[1:ndim(source)]))
            x$scales <- sign(x$scales)
        }
    }
    else
        target <- asNifti(target, internal=TRUE)
    
    if (ndim(source) != ndim(target))
        stop("Source and target images must be of the same dimensionality")
    nDims <- ndim(source)
    
    anchor <- match.arg(anchor)
    
    affine <- diag(4)
    
    rotationX <- rotationY <- rotationZ <- skewMatrix <- diag(3)
    cosAngles <- cos(x$angles)
    sinAngles <- sin(x$angles)
    rotationX[2:3,2:3] <- c(cosAngles[1], -sinAngles[1], sinAngles[1], cosAngles[1])
    rotationY[c(1,3),c(1,3)] <- c(cosAngles[2], sinAngles[2], -sinAngles[2], cosAngles[2])
    rotationZ[1:2,1:2] <- c(cosAngles[3], -sinAngles[3], sinAngles[3], cosAngles[3])
    skewMatrix[c(4,7,8)] <- x$skews
    
    affine[1:3,1:3] <- rotationX %*% rotationY %*% rotationZ %*% skewMatrix %*% diag(x$scales)
    affine[1:3,4] <- x$translation
    
    affine <- solve(affine)
    
    if (anchor == "origin")
        affine[,4] <- affine[,4] - (affine %*% c(0,0,0,1))
    else if (anchor %in% c("centre","center"))
    {
        sourceCentre <- voxelToWorld((dim(source)+1)/2, source)
        targetCentre <- voxelToWorld((dim(target)+1)/2, target)
        affine[,4] <- affine[,4] + c(sourceCentre,rep(1,4-nDims)) - (affine %*% c(targetCentre,rep(1,4-nDims)))
    }
    
    affine[4,] <- c(0,0,0,1)
    
    return (asAffine(affine, source, target))
}


#' Decompose an affine matrix into its constituent transformations
#' 
#' An affine matrix is composed of translation, scale, skew and rotation
#' transformations. This function extracts these components, after first
#' inverting the matrix so that it transforms from source to target space.
#' 
#' @param affine A 4x4 matrix representing an affine transformation matrix.
#' @return A list with components:
#'   \describe{
#'     \item{scaleMatrix}{A 3x3 matrix representing only the scale operation
#'       embodied in the full affine transformation.}
#'     \item{skewMatrix}{A 3x3 matrix representing only the skew operation
#'       embodied in the full affine transformation.}
#'     \item{rotationMatrix}{A 3x3 matrix representing only the rotation
#'       operation embodied in the full affine transformation.}
#'     \item{translation}{A length-3 named numeric vector representing the
#'       translations (in \code{\link{pixunits}} units) in each of the X, Y and
#'       Z directions.}
#'     \item{scales}{A length-3 named numeric vector representing the scale
#'       factors in each of the X, Y and Z directions. Scale factors of 1
#'       represent no effect.}
#'     \item{skews}{A length-3 named numeric vector representing the skews in
#'       each of the XY, XZ and YZ planes.}
#'     \item{angles}{A length-3 named numeric vector representing the rotation
#'       angles (in radians) about each of the X, Y and Z directions, i.e.,
#'       roll, pitch and yaw.}
#'   }
#' 
#' @note The decomposition is not perfect, and there is one particular
#'   degenerate case when the pitch angle is very close to \code{pi/2} radians,
#'   known as ``Gimbal lock''. In this case the yaw angle is arbitrarily set to
#'   zero.
#'   
#'   Affine matrices embodying rigid-body transformations include only 6
#'   degrees of freedom, rather than the full 12, so skews will always be zero
#'   and scales will always be unity (to within rounding error). Likewise,
#'   affine matrices derived from 2D registration will not include components
#'   relating to the Z direction.
#' 
#' @author Jon Clayden <code@@clayden.org>
#' @seealso \code{\link{buildAffine}}, \code{\link{isAffine}}
#' @export
decomposeAffine <- function (affine)
{
    if (!isAffine(affine))
        stop("Specified affine matrix is not valid")
    
    affine <- solve(affine)
    
    # Full matrix is rotationX %*% rotationY %*% rotationZ %*% skew %*% scale
    # The Cholesky decomposition strategy is due to Tim Tierney
    submatrix <- affine[1:3,1:3]
    decomposition <- chol(crossprod(submatrix))
    scales <- diag(decomposition)
    scaleMatrix <- diag(scales)
    skewMatrix <- decomposition %*% solve(scaleMatrix)
    skews <- skewMatrix[c(4,7,8)]
    rotationMatrix <- submatrix %*% solve(scaleMatrix) %*% solve(skewMatrix)
    translation <- affine[1:3,4]
    
    pitchAngle <- asin(-rotationMatrix[1,3])
    if (cos(pitchAngle) < 1e-4)
    {
        # Degenerate case (Gimbal lock) - fix yaw angle at zero
        rollAngle <- atan2(-rotationMatrix[3,2], rotationMatrix[2,2])
        yawAngle <- 0
    }
    else
    {
        rollAngle <- atan2(rotationMatrix[2,3], rotationMatrix[3,3])
        yawAngle <- atan2(rotationMatrix[1,2], rotationMatrix[1,1])
    }
    angles <- c(rollAngle, pitchAngle, yawAngle)
    
    names(translation) <- letters[24:26]
    names(scales) <- letters[24:26]
    names(skews) <- c("xy", "xz", "yz")
    names(angles) <- c("roll", "pitch", "yaw")
    
    return (list(scaleMatrix=scaleMatrix, skewMatrix=skewMatrix, rotationMatrix=rotationMatrix, translation=translation, scales=scales, skews=skews, angles=angles))
}
