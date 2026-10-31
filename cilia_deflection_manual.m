
function results = cilia_deflection_manual()
% CILIA_DEFLECTION_MANUAL
%
%Run using: results = cilia_deflection_manual();
%
% Manual deflection measurement for ONE cilium per test.
%
% Outputs:
%   - Tip displacement in pixels and mm (primary measurement)
%   - Horizontal and vertical tip displacement in pixels and mm
%   - Tip-to-base angle change in degrees
%   - Base tangent angle change in degrees
%   - Original and deflected centreline lengths
%   - Per-test calibration factor in mm/pixel
%
% Assumptions:
%   - Original and deflected images are captured with the same camera
%     position and magnification within each test.
%   - Cilium bending occurs approximately in the image plane.
%   - Known physical length is the visible/exposed cilium length.
%   - Centreline points are clicked in order from base to tip.
%
% This script does not automatically correct image shifts. If the camera
% or sample moves between the two images, register them before measuring.

clc;
fprintf('\nManual magnetic cilium deflection measurement\n');

% ---------- 1. Load images ----------
[f0,p0] = uigetfile( ...
    {'*.png;*.jpg;*.jpeg;*.tif;*.tiff;*.bmp','Image files'}, ...
    'Select ORIGINAL cilium image');

if isequal(f0,0)
    results = table();
    return;
end

originalPath = fullfile(p0,f0);
I0 = imread(originalPath);

[fd,pd] = uigetfile( ...
    {'*.png;*.jpg;*.jpeg;*.tif;*.tiff;*.bmp','Image files'}, ...
    'Select DEFLECTED cilium image');

if isequal(fd,0)
    results = table();
    return;
end

deflectedPath = fullfile(pd,fd);
Id = imread(deflectedPath);

if size(I0,1) ~= size(Id,1) || size(I0,2) ~= size(Id,2)
    error(['The images have different pixel dimensions. ' ...
        'Align/resample them to a common image size before measuring.']);
end

% ---------- 2. Enter known physical length ----------
answer = inputdlg( ...
    {'Known EXPOSED cilium length (mm)', ...
     'Test ID'}, ...
    'Cilium information', [1 55], {'','Test01'});

if isempty(answer)
    results = table();
    return;
end

knownLengthMM = str2double(answer{1});
testID = string(answer{2});

if ~isfinite(knownLengthMM) || knownLengthMM <= 0
    error('Known exposed cilium length must be a positive number in mm.');
end

% ---------- 3. Select original base, tip and centreline ----------
[base0,tip0,curve0] = selectCilium( ...
    I0,'ORIGINAL: click BASE, TIP, then centreline points base-to-tip');

% Original centreline length in pixels
originalLengthPX = polylineLength(curve0);

if ~isfinite(originalLengthPX) || originalLengthPX <= 0
    error('Could not calculate original centreline length.');
end

% Per-test scale: known exposed length divided by original arc length
scaleMMperPX = knownLengthMM/originalLengthPX;

fprintf('\nCalibration for %s:\n',testID);
fprintf('Known exposed length: %.6f mm\n',knownLengthMM);
fprintf('Original traced length: %.3f px\n',originalLengthPX);
fprintf('Scale: %.9f mm/px\n',scaleMMperPX);

% ---------- 4. Select deflected base, tip and centreline ----------
[based,tipd,curved] = selectCilium( ...
    Id,'DEFLECTED: click BASE, TIP, then centreline points base-to-tip');

% ---------- 5. Calculate tip displacement ----------
% Tip displacement vector in image coordinates.
% Image x increases rightward; image y increases downward.
deltaXpx = tipd(1)-tip0(1);
deltaYpx = tipd(2)-tip0(2);

% Euclidean magnitude: primary result
tipDisplacementPX = hypot(deltaXpx,deltaYpx);
tipDisplacementMM = tipDisplacementPX*scaleMMperPX;

% Signed components
deltaXmm = deltaXpx*scaleMMperPX;
deltaYmm = deltaYpx*scaleMMperPX;

% ---------- 6. Calculate angular measurements ----------
% Cartesian convention: positive angle is counter-clockwise.
initialTipAngle = atan2d( ...
    -(tip0(2)-base0(2)), tip0(1)-base0(1));

finalTipAngle = atan2d( ...
    -(tipd(2)-based(2)), tipd(1)-based(1));

tipAngleChange = wrapAngle(finalTipAngle-initialTipAngle);

initialBaseAngle = estimateBaseTangent(curve0);
finalBaseAngle = estimateBaseTangent(curved);
baseAngleChange = wrapAngle(finalBaseAngle-initialBaseAngle);

% ---------- 7. Centreline lengths ----------
deflectedLengthPX = polylineLength(curved);

% Convert using the ORIGINAL image calibration.
% Do not recalibrate from the deflected image, since its projected arc
% length can change with bending.
originalLengthMM = originalLengthPX*scaleMMperPX;
deflectedLengthMM = deflectedLengthPX*scaleMMperPX;

% ---------- 8. Create results table ----------
results = table( ...
    testID, knownLengthMM, ...
    originalLengthPX, originalLengthMM, ...
    scaleMMperPX, ...
    base0(1),base0(2),tip0(1),tip0(2), ...
    based(1),based(2),tipd(1),tipd(2), ...
    deltaXpx,deltaYpx,tipDisplacementPX, ...
    deltaXmm,deltaYmm,tipDisplacementMM, ...
    initialTipAngle,finalTipAngle,tipAngleChange, ...
    initialBaseAngle,finalBaseAngle,baseAngleChange, ...
    deflectedLengthPX,deflectedLengthMM, ...
    'VariableNames',{ ...
    'TestID','KnownExposedLength_mm', ...
    'OriginalCentreline_px','OriginalCentreline_mm', ...
    'Scale_mm_per_px', ...
    'OriginalBaseX_px','OriginalBaseY_px', ...
    'OriginalTipX_px','OriginalTipY_px', ...
    'DeflectedBaseX_px','DeflectedBaseY_px', ...
    'DeflectedTipX_px','DeflectedTipY_px', ...
    'DeltaX_px','DeltaY_px','TipDisplacement_px', ...
    'DeltaX_mm','DeltaY_mm','TipDisplacement_mm', ...
    'InitialTipAngle_deg','FinalTipAngle_deg','TipAngleChange_deg', ...
    'InitialBaseTangent_deg','FinalBaseTangent_deg', ...
    'BaseTangentChange_deg', ...
    'DeflectedCentreline_px','DeflectedCentreline_mm'});

% ---------- 9. Show primary result ----------
fprintf('\n========== RESULTS ==========\n');
fprintf('Test ID: %s\n',testID);
fprintf('Tip displacement: %.3f px\n',tipDisplacementPX);
fprintf('Tip displacement: %.6f mm\n',tipDisplacementMM);
fprintf('Delta X: %.6f mm\n',deltaXmm);
fprintf('Delta Y: %.6f mm (positive is downward in image)\n',deltaYmm);
fprintf('Tip-to-base angle change: %.3f degrees\n',tipAngleChange);
fprintf('Base tangent angle change: %.3f degrees\n',baseAngleChange);
fprintf('=============================\n');

disp(results);

% ---------- 10. Save results ----------
outDir = uigetdir(pwd,'Select output folder');

if isequal(outDir,0)
    warning('No output folder selected. Results remain in workspace.');
    assignin('base','ciliaResults',results);
    return;
end

[~,stem0] = fileparts(originalPath);
[~,stemD] = fileparts(deflectedPath);
stem = matlab.lang.makeValidName([char(testID) '_' stem0 '_' stemD]);

csvPath = fullfile(outDir,[stem '_measurements.csv']);
matPath = fullfile(outDir,[stem '_data.mat']);
originalPNG = fullfile(outDir,[stem '_original_annotated.png']);
deflectedPNG = fullfile(outDir,[stem '_deflected_annotated.png']);

writetable(results,csvPath);

save(matPath,'results','curve0','curved','base0','tip0', ...
    'based','tipd','knownLengthMM','scaleMMperPX', ...
    'originalPath','deflectedPath');

% Create annotated images using standard graphics (no Computer Vision
% Toolbox required).
saveAnnotatedImage(I0,base0,tip0,curve0, ...
    originalPNG,'Original image');

saveAnnotatedImage(Id,based,tipd,curved, ...
    deflectedPNG,'Deflected image');

fprintf('\nSaved files to:\n%s\n',outDir);
fprintf('CSV: %s\n',csvPath);
fprintf('MAT: %s\n',matPath);
fprintf('Original annotation: %s\n',originalPNG);
fprintf('Deflected annotation: %s\n',deflectedPNG);

assignin('base','ciliaResults',results);

end

% ========================================================================
function [basePt,tipPt,curvePts] = selectCilium(I,figTitle)

fig = figure('Name',figTitle,'NumberTitle','off','Color','w');
imshow(I);
title({figTitle, ...
    'Click BASE, then TIP. Then click centreline points base-to-tip.', ...
    'Press Enter when finished.'},'Interpreter','none');
hold on;

[x,y] = ginput(2);

if numel(x) < 2
    close(fig);
    error('You must select both base and tip.');
end

basePt = [x(1),y(1)];
tipPt = [x(2),y(2)];

plot(basePt(1),basePt(2),'go','MarkerSize',9,'LineWidth',2);
plot(tipPt(1),tipPt(2),'ro','MarkerSize',9,'LineWidth',2);

text(basePt(1)+5,basePt(2),'BASE', ...
    'Color','g','FontWeight','bold');
text(tipPt(1)+5,tipPt(2),'TIP', ...
    'Color','r','FontWeight','bold');

title({'Click points along the centreline from base toward tip.', ...
    'Press Enter when finished.'});

[xc,yc] = ginput;

% Include exact base and tip as endpoints.
curvePts = [basePt; [xc(:),yc(:)]; tipPt];

% Remove consecutive duplicate points.
if size(curvePts,1) > 1
    keep = [true; hypot(diff(curvePts(:,1)), ...
        diff(curvePts(:,2))) > 0];
    curvePts = curvePts(keep,:);
end

plot(curvePts(:,1),curvePts(:,2), ...
    'y.-','LineWidth',1.5,'MarkerSize',12);

title('Review yellow centreline; close window when satisfied.');
hold off;

uiwait(msgbox( ...
    'Check the yellow centreline. Click OK to accept it.', ...
    'Confirm centreline','modal'));

if isvalid(fig)
    close(fig);
end

end

% ========================================================================
function L = polylineLength(points)

if size(points,1) < 2
    L = NaN;
else
    L = sum(hypot(diff(points(:,1)),diff(points(:,2))));
end

end

% ========================================================================
function angleDeg = estimateBaseTangent(curvePts)
% Estimate base tangent from the first few ordered centreline points.

n = min(size(curvePts,1),4);
pts = curvePts(1:n,:);

if size(pts,1) < 2
    angleDeg = NaN;
    return;
end

centered = pts-mean(pts,1);
[~,~,V] = svd(centered,0);
direction = V(:,1)';

% Ensure direction points from base toward tip.
if dot(direction,pts(end,:)-pts(1,:)) < 0
    direction = -direction;
end

% Convert downward-positive image coordinates to Cartesian convention.
angleDeg = atan2d(-direction(2),direction(1));

end

% ========================================================================
function a = wrapAngle(a)
% Wrap angle to [-180,180).

a = mod(a+180,360)-180;

end

% ========================================================================
function saveAnnotatedImage(I,basePt,tipPt,curvePts,outPath,plotTitle)
% Use standard MATLAB plotting to save annotated image without toolboxes.

fig = figure('Visible','off','Color','w');
imshow(I);
hold on;

plot(curvePts(:,1),curvePts(:,2), ...
    'y.-','LineWidth',1.5,'MarkerSize',12);

plot(basePt(1),basePt(2),'go','MarkerSize',9,'LineWidth',2);
plot(tipPt(1),tipPt(2),'ro','MarkerSize',9,'LineWidth',2);

quiver(tipPt(1),tipPt(2), ...
    tipPt(1)-basePt(1),tipPt(2)-basePt(2), ...
    0,'Color','c','LineWidth',1.5);

title(plotTitle);
hold off;

try
    exportgraphics(gca,outPath,'Resolution',200);
catch
    % Compatibility fallback for older MATLAB versions.
    frame = getframe(gca);
    imwrite(frame.cdata,outPath);
end

close(fig);

end

