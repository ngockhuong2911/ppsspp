// Copyright (c) 2013- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <vector>

#include "base/colorutil.h"
#include "gfx_es2/draw_buffer.h"
#include "i18n/i18n.h"
#include "ui/ui_context.h"
#include "ui/view.h"
#include "ui_atlas.h"

#include "DisplayLayoutScreen.h"
#include "Core/Config.h"
#include "Core/System.h"
#include "DisplayLayoutEditor.h"
#include "GPU/GLES/Framebuffer.h"

static const int leftColumnWidth = 200;
static const float orgRatio = 1.764706;

// Ugly hackery, need to rework some stuff to get around this
static float local_dp_xres;
static float local_dp_yres;

class DragDropDisplay : public MultiTouchDisplay {
public:
	DragDropDisplay(float &x, float &y, int img, float &scale)
		: MultiTouchDisplay(img, scale, new UI::AnchorLayoutParams(x*local_dp_xres, y*local_dp_yres, UI::NONE, UI::NONE, true)),
		x_(x), y_(y), theScale_(scale) {
		scale_ = theScale_;
	}	

	virtual void SaveDisplayPosition() {
		x_ = bounds_.centerX() / local_dp_xres;
		y_ = bounds_.centerY() / local_dp_yres;
		scale_ = theScale_;
	}

	virtual float GetScale() const { return theScale_; }
	virtual void SetScale(float s) { theScale_ = s; scale_ = s; }

	private:

	float &x_, &y_;
	float &theScale_;
};

DisplayLayoutScreen::DisplayLayoutScreen() {
	picked_ = 0;
};


bool DisplayLayoutScreen::touch(const TouchInput &touch) {
	UIScreen::touch(touch);

	using namespace UI;

	int mode = mode_->GetSelection();
	if (g_Config.iSmallDisplayZoomType == 2) { mode = -1; }

	const Bounds &screen_bounds = screenManager()->getUIContext()->GetBounds();
	if ((touch.flags & TOUCH_MOVE) && picked_ != 0) {
		int touchX = touch.x - offsetTouchX;
		int touchY = touch.y - offsetTouchY;
		if (mode == 0) {
			const Bounds &bounds = picked_->GetBounds();

			int minTouchX = screen_bounds.w / 4;
			int maxTouchX = screen_bounds.w - minTouchX;

			int minTouchY = screen_bounds.h / 4;
			int maxTouchY = screen_bounds.h - minTouchY;

			int newX = bounds.centerX(), newY = bounds.centerY();
			// we have to handle x and y separately since even if x is blocked, y may not be.
			if (touchX > minTouchX && touchX < maxTouchX) {
				// if the leftmost point of the control is ahead of the margin,
				// move it. Otherwise, don't.
				newX = touchX;
			}
			if (touchY > minTouchY && touchY < maxTouchY) {
				newY = touchY;
			}
			picked_->ReplaceLayoutParams(new UI::AnchorLayoutParams(newX, newY, NONE, NONE, true));
		} else if (mode == 1) {
			// Resize. Vertical = scaling, horizontal = spacing;
			// Up should be bigger so let's negate in that direction
			float diffX = (touchX - startX_);
			float diffY = -(touchY - startY_);

			float movementScale = 0.5f;
			float newScale = startScale_ + diffY * movementScale;
			if (newScale > 100.0f) newScale = 100.0f;
			if (newScale < 1.0f) newScale = 1.0f;
			picked_->SetScale(newScale);
			scaleUpdate_ = picked_->GetScale();
			g_Config.fSmallDisplayZoomLevel = scaleUpdate_ / 8.0f;
		}
	}
	if ((touch.flags & TOUCH_DOWN) && picked_ == 0) {
		picked_ = displayRepresentation_;
		if (picked_) {
			const Bounds &bounds = picked_->GetBounds();
			startX_ = bounds.centerX();
			startY_ = bounds.centerY();
			offsetTouchX = touch.x - startX_;
			offsetTouchY = touch.y - startY_;
			startScale_ = picked_->GetScale();
		}
	}
	if ((touch.flags & TOUCH_UP) && picked_ != 0) {
		const Bounds &bounds = picked_->GetBounds();
		float saveX_ = touch.x;
		float saveY_ = touch.y;
		startScale_ = picked_->GetScale();
		picked_->SaveDisplayPosition();
		picked_ = 0;
	}
	return true;
};

void DisplayLayoutScreen::onFinish(DialogResult reason) {
	g_Config.Save();
}

UI::EventReturn DisplayLayoutScreen::OnCenter(UI::EventParams &e) {
	g_Config.fSmallDisplayOffsetX = 0.5f;
	g_Config.fSmallDisplayOffsetY = 0.5f;
	RecreateViews();
	return UI::EVENT_DONE;
};

UI::EventReturn DisplayLayoutScreen::OnZoomTypeChange(UI::EventParams &e) {
	if (g_Config.iSmallDisplayZoomType < 3) {
		const Bounds &bounds = screenManager()->getUIContext()->GetBounds();
		float autoBound = bounds.w / 480.0f;
		g_Config.fSmallDisplayZoomLevel = autoBound;
		displayRepresentationScale_ = g_Config.fSmallDisplayZoomLevel * 8.0f;
		g_Config.fSmallDisplayOffsetX = 0.5f;
		g_Config.fSmallDisplayOffsetY = 0.5f;
	}
	RecreateViews();
	return UI::EVENT_DONE;
};

void DisplayLayoutScreen::dialogFinished(const Screen *dialog, DialogResult result) {
	RecreateViews();
}

void DisplayLayoutScreen::CreateViews() {
	const Bounds &bounds = screenManager()->getUIContext()->GetBounds();

	local_dp_xres = bounds.w;
	local_dp_yres = bounds.h;

	using namespace UI;

	I18NCategory *di = GetI18NCategory("Dialog");
	I18NCategory *gr = GetI18NCategory("Graphics");
	I18NCategory *co = GetI18NCategory("Controls");

	root_ = new AnchorLayout(new LayoutParams(FILL_PARENT, FILL_PARENT));

	// Just visual boundaries of the screen, should be easier to use than imagination
	float verticalBoundaryPositionL = local_dp_xres / 4.0f;
	float verticalBoundaryPositionR = local_dp_xres - verticalBoundaryPositionL;
	float horizontalBoundaryPositionL = local_dp_yres / 4.0f;
	float horizontalBoundaryPositionR = local_dp_yres - horizontalBoundaryPositionL;
	TabHolder *verticalBoundaryL = new TabHolder(ORIENT_VERTICAL, verticalBoundaryPositionL, new AnchorLayoutParams(0, 0, 0, 0, false));
	TabHolder *verticalBoundaryR = new TabHolder(ORIENT_VERTICAL, verticalBoundaryPositionR + 4.0f, new AnchorLayoutParams(0, 0, 0, 0, false));
	TabHolder *horizontalBoundaryL = new TabHolder(ORIENT_VERTICAL, verticalBoundaryPositionL * 2.0f, new AnchorLayoutParams(verticalBoundaryPositionL * 2.0f, horizontalBoundaryPositionL - 32.0f, 0, 0, true));
	TabHolder *horizontalBoundaryR = new TabHolder(ORIENT_VERTICAL, verticalBoundaryPositionL * 2.0f, new AnchorLayoutParams(verticalBoundaryPositionL * 2.0f, horizontalBoundaryPositionR + 32.0f, 0, 0, true));
	AnchorLayout *topBoundary = new AnchorLayout(new LayoutParams(FILL_PARENT, FILL_PARENT));
	AnchorLayout *bottomBoundary = new AnchorLayout(new LayoutParams(FILL_PARENT, FILL_PARENT));
	root_->Add(verticalBoundaryL);
	root_->Add(verticalBoundaryR);
	root_->Add(horizontalBoundaryL);
	root_->Add(horizontalBoundaryR);
	horizontalBoundaryL->AddTab("", topBoundary);
	horizontalBoundaryR->AddTab("", bottomBoundary);

	Choice *back = new Choice(di->T("Back"), "", false, new AnchorLayoutParams(leftColumnWidth, WRAP_CONTENT, 10, NONE, NONE, 10));
	static const char *zoomLevels[] = { "Stretching", "Partial Stretch", "Auto Scaling", "Manual Scaling" };
	zoom_ = new PopupMultiChoice(&g_Config.iSmallDisplayZoomType, di->T("Options"), zoomLevels, 0, ARRAY_SIZE(zoomLevels), gr->GetName(), screenManager(), new AnchorLayoutParams(400, WRAP_CONTENT, local_dp_xres / 2 - 200, NONE, NONE, 10));
	zoom_->OnChoice.Handle(this, &DisplayLayoutScreen::OnZoomTypeChange);

	static const char *displayRotation[] = { "Landscape", "Portrait", "Landscape Reversed", "Portrait Reversed" };
	rotation_ = new PopupMultiChoice(&g_Config.iInternalScreenRotation, gr->T("Rotation"), displayRotation, 1, ARRAY_SIZE(displayRotation), co->GetName(), screenManager(), new AnchorLayoutParams(400, WRAP_CONTENT, local_dp_xres / 2 - 200, NONE, NONE, local_dp_yres - 64));
	rotation_->SetEnabledPtr(&displayRotEnable_);
	displayRotEnable_ = (g_Config.iRenderingMode != FB_NON_BUFFERED_MODE);
	bool bRotated = false;
	if (displayRotEnable_ && (g_Config.iInternalScreenRotation == ROTATION_LOCKED_VERTICAL || g_Config.iInternalScreenRotation == ROTATION_LOCKED_VERTICAL180)) {
		bRotated = true;
	}
	mode_ = new ChoiceStrip(ORIENT_VERTICAL, new AnchorLayoutParams(leftColumnWidth, WRAP_CONTENT, 10, NONE, NONE, 158 + 64 + 10));
	displayRepresentationScale_ = g_Config.fSmallDisplayZoomLevel * 8.0f; // Visual representation image is just icon size and have to be scaled 8 times to match PSP native resolution which is used as 1.0 for zoom
	if (g_Config.iSmallDisplayZoomType > 1) { // Scaling
		if (g_Config.iSmallDisplayZoomType == 2) { // Auto Scaling
			mode_->AddChoice(gr->T("Auto Scaling"));
			mode_->ReplaceLayoutParams(new AnchorLayoutParams(0, 0, local_dp_xres / 2.0f - 70.0f, NONE, NONE, local_dp_yres / 2.0f + 32.0f));
			float autoBound = local_dp_yres / 270.0f;
			// Case of screen rotated ~ only works with buffered rendering
			if (bRotated) {
				autoBound = local_dp_yres / 480.0f;
			}
			else { // Without rotation in common cases like 1080p we cut off 2 pixels of height, this reflects other cases
				float resCommonWidescreen = autoBound - floor(autoBound);
				if (resCommonWidescreen != 0.0f) {
					float ratio = local_dp_xres / local_dp_yres;
					if (ratio < orgRatio) {
						autoBound = local_dp_xres / 480.0f;
					}
					else {
						autoBound = local_dp_yres / 272.0f;
					}
				}
			}
			g_Config.fSmallDisplayZoomLevel = autoBound;
			displayRepresentationScale_ = g_Config.fSmallDisplayZoomLevel * 8.0f;
			g_Config.fSmallDisplayOffsetX = 0.5f;
			g_Config.fSmallDisplayOffsetY = 0.5f;
		} else { // Manual Scaling
			Choice *center = new Choice(di->T("Center"), "", false, new AnchorLayoutParams(leftColumnWidth, WRAP_CONTENT, 10, NONE, NONE, 74));
			center->OnClick.Handle(this, &DisplayLayoutScreen::OnCenter);
			root_->Add(center);
			PopupSliderChoiceFloat *zoomlvl_ = new PopupSliderChoiceFloat(&g_Config.fSmallDisplayZoomLevel, 1.0f, 10.0f, di->T("Zoom"), 1.0f, screenManager(), di->T("* PSP res"), new AnchorLayoutParams(leftColumnWidth, WRAP_CONTENT, 10, NONE, NONE, 134));
			root_->Add(zoomlvl_);
			mode_->AddChoice(di->T("Move"));
			mode_->AddChoice(di->T("Resize"));
			mode_->SetSelection(0);
		}
		displayRepresentation_ = new DragDropDisplay(g_Config.fSmallDisplayOffsetX, g_Config.fSmallDisplayOffsetY, I_PSP_DISPLAY, displayRepresentationScale_);
		displayRepresentation_->SetVisibility(V_VISIBLE);
	} else { // Stretching
		mode_->AddChoice(gr->T("Stretching"));
		mode_->ReplaceLayoutParams(new AnchorLayoutParams(0, 0, local_dp_xres / 2.0f - 70.0f, NONE, NONE, local_dp_yres / 2.0f + 32.0f));
		displayRepresentation_ = new DragDropDisplay(g_Config.fSmallDisplayOffsetX, g_Config.fSmallDisplayOffsetY, I_PSP_DISPLAY, displayRepresentationScale_);
		displayRepresentation_->SetVisibility(V_INVISIBLE);
		float width = local_dp_xres / 2.0f;
		float height = local_dp_yres / 2.0f;
		if (g_Config.iSmallDisplayZoomType == 0) { // Stretched
			Choice *stretched = new Choice("", "", false, new AnchorLayoutParams(width, height, width - width / 2.0f, NONE, NONE, height - height / 2.0f));
			stretched->SetEnabled(false);
			root_->Add(stretched);
		} else { // Partially stretched
			float origRatio = !bRotated ? 480.0f / 272.0f : 272.0f / 480.0f;
			float frameRatio = width / height;
			if (origRatio > frameRatio) {
				height = width / origRatio;
				if (!bRotated && g_Config.iSmallDisplayZoomType == 1) {	height = (272.0f + height) / 2.0f; }
			} else {
				width = height * origRatio;
				if (bRotated && g_Config.iSmallDisplayZoomType == 1) { width = (272.0f + height) / 2.0f; }
			}
			Choice *stretched = new Choice("", "", false, new AnchorLayoutParams(width, height, local_dp_xres / 2.0f - width / 2.0f, NONE, NONE, local_dp_yres / 2.0f - height / 2.0f));
			stretched->SetEnabled(false);
			root_->Add(stretched);
		}
	}
	if (bRotated) {
		displayRepresentation_->SetAngle(90.0f);
	}

	back->OnClick.Handle<UIScreen>(this, &UIScreen::OnBack);
	root_->Add(displayRepresentation_);
	root_->Add(mode_);
	root_->Add(zoom_);
	root_->Add(rotation_);
	root_->Add(back);
}
