/*
	Spinner.cpp: A number spinner control.
	Written by DarkWyrm <darkwyrm@earthlink.net>, Copyright 2004
	Released under the MIT license.
	
	Original BScrollBarButton class courtesy Haiku project
*/
#include "Spinner.h"
#include <AbstractLayoutItem.h>
#include <ControlLook.h>
#include <LayoutUtils.h>
#include <MenuItem.h>
#include <String.h>
#include <ScrollBar.h>
#include <Window.h>
#include <stdio.h>
#include <Font.h>
#include <Box.h>
#include <MessageFilter.h>
#include <PropertyInfo.h>

#include <algorithm>

#include "Thread.h"

#include <math.h>

static property_info sProperties[] = {
	{ "MinValue", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Returns the minimum value for the spinner.", 0, { B_INT32_TYPE }
	},
	
	{ "MinValue", { B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0},
		"Sets the minimum value for the spinner.", 0, { B_INT32_TYPE }
	},
	
	{ "MaxValue", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Returns the maximum value for the spinner.", 0, { B_INT32_TYPE }
	},
	
	{ "MaxValue", { B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0},
		"Sets the maximum value for the spinner.", 0, { B_INT32_TYPE }
	},
	
	{ "Step", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Returns the amount of change when an arrow button is clicked.",
		0, { B_INT32_TYPE }
	},
	
	{ "Step", { B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0},
		"Sets the amount of change when an arrow button is clicked.",
		0, { B_INT32_TYPE }
	},
	
	{ "Value", { B_GET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0 },
		"Returns the value for the spinner.", 0, { B_INT32_TYPE }
	},
	
	{ "Value", { B_SET_PROPERTY, 0 }, { B_DIRECT_SPECIFIER, 0},
		"Sets the value for the spinner.", 0, { B_INT32_TYPE }
	},
};

enum {
	M_UP = 'mmup',
	M_DOWN,
	M_TEXT_CHANGED = 'mtch'
};


typedef enum {
	ARROW_LEFT = 0,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	ARROW_NONE
} arrow_direction;


const char* const kFrameField = "Spinner:layoutItem:frame";
const char*	const kLabelItemField = "Spinner:textFieldItem";
const char* const kTextFieldItemField = "Spinner:labelItem";


class SpinnerMsgFilter : public BMessageFilter
{
public:
							SpinnerMsgFilter(void);
							~SpinnerMsgFilter(void);
	virtual filter_result	Filter(BMessage *msg, BHandler **target);
};


class SpinnerArrowButton : public BView
{
public:
							SpinnerArrowButton(BPoint location, const char *name, 
								arrow_direction dir, float height);
							~SpinnerArrowButton(void);
					void	AttachedToWindow(void);
					void	DetachedToWindow(void);
					void	MouseDown(BPoint pt);
					void	MouseUp(BPoint pt);
					void	MouseMoved(BPoint pt, uint32 code, const BMessage *msg);
					void	Draw(BRect update);
					void	SetEnabled(bool value);
					bool	IsEnabled(void) const { return fEnabled; }
	
private:
		arrow_direction		fDirection;
		float				fHeight;
		Spinner				*fParent;
		bool				fMouseDown;
		bool				fEnabled;
		bool				fMouseOver;

private:
		void				_DoneTracking(BPoint point);
		void				_Track(BPoint point, uint32);
		void				_ModifyValue();
};

class SpinnerPrivateData
{
public:
	SpinnerPrivateData(void)
	{
		fThumbFrame.Set(0,0,0,0);
		fEnabled = true;
		tracking = false;
		fMousePoint.Set(0,0);
		fThumbIncrement = 1.0;
		fRepeaterID = -1;
		fExitRepeater = false;
		fArrowDown = ARROW_NONE;
		
		#ifdef TEST_MODE
			sbinfo.proportional = true;
			sbinfo.double_arrows = false;
			sbinfo.knob = 0;
			sbinfo.min_knob_size = 14;
		#else
			get_scroll_bar_info(&fScrollbarInfo);
		#endif
	}
	
	~SpinnerPrivateData(void)
	{
		if (fRepeaterID != -1)
		{
			fExitRepeater = false;
			kill_thread(fRepeaterID);
		}
	}
	
	static	int32	ButtonRepeaterThread(void *data);
	
			thread_id 		fRepeaterID;
			scroll_bar_info	fScrollbarInfo;
			BRect			fThumbFrame;
			bool			fEnabled;
			bool			tracking;
			BPoint			fMousePoint;
			float			fThumbIncrement;
			bool			fExitRepeater;
			arrow_direction	fArrowDown;
};


class Spinner::LabelLayoutItem : public BAbstractLayoutItem {
public:
							LabelLayoutItem(Spinner* parent);
							LabelLayoutItem(BMessage* archive);
							
	virtual	bool			IsVisible();
	virtual	void			SetVisible(bool visible);
	
	virtual	BRect			Frame();
	virtual	void			SetFrame(BRect frame);
	
			void			SetParent(Spinner* parent);
	virtual	BView*			View();
	
	virtual BSize			BaseMinSize();
	virtual BSize			BaseMaxSize();
	virtual	BSize			BasePreferredSize();
	virtual	BAlignment		BaseAlignment();
	
	virtual status_t		Archive(BMessage* into, bool deep = true) const;
	static	BArchivable*	Instantiate(BMessage* from);
	
private:
			Spinner*		fParent;
			BRect			fFrame;
};


class Spinner::TextFieldLayoutItem : public BAbstractLayoutItem {
public:
							TextFieldLayoutItem(Spinner* parent);
							TextFieldLayoutItem(BMessage* archive);
							
	virtual	bool			IsVisible();
	virtual	void			SetVisible(bool visible);
	
	virtual	BRect			Frame();
	virtual	void			SetFrame(BRect frame);
	
			void			SetParent(Spinner* parent);
	virtual	BView*			View();
	
	virtual BSize			BaseMinSize();
	virtual BSize			BaseMaxSize();
	virtual	BSize			BasePreferredSize();
	virtual	BAlignment		BaseAlignment();
	
	virtual status_t		Archive(BMessage* into, bool deep = true) const;
	static	BArchivable*	Instantiate(BMessage* from);
	
private:
			Spinner*		fParent;
			BRect			fFrame;
};


struct Spinner::LayoutData {
	LayoutData()
		:
		labelLayoutItem(NULL),
		textFieldLayoutItem(NULL),
		previousHeight(-1),
		valid(false)
	{
	}
	
	BRect					labelBox;
	
	LabelLayoutItem*		labelLayoutItem;
	TextFieldLayoutItem*	textFieldLayoutItem;
	float					previousHeight;
	
	font_height				fontInfo;
	float					labelWidth;
	float					labelHeight;
	BSize					textFieldHeight;
	BSize					textFieldMin;
	BSize					minSize;
	BSize					maxSize;
	BSize					min;
	BAlignment				alignment;
	bool					valid;
};


Spinner::LabelLayoutItem::LabelLayoutItem(Spinner* parent)
	:
	fParent(parent),
	fFrame()
{
}


Spinner::LabelLayoutItem::LabelLayoutItem(BMessage* from)
	:
	BAbstractLayoutItem(from),
	fParent(NULL),
	fFrame()
{
	from->FindRect(kFrameField, &fFrame);
}


bool
Spinner::LabelLayoutItem::IsVisible()
{
	return !fParent->IsHidden(fParent);	
}


void
Spinner::LabelLayoutItem::SetVisible(bool visible)
{
}


BRect
Spinner::LabelLayoutItem::Frame()
{
	return fFrame;	
}


void
Spinner::LabelLayoutItem::SetFrame(BRect frame)
{
	fFrame = frame;
	fParent->_UpdateFrame();	
}


void
Spinner::LabelLayoutItem::SetParent(Spinner* parent)
{
	fParent = parent;	
}


BView*
Spinner::LabelLayoutItem::View()
{
	return fParent;
}


BSize
Spinner::LabelLayoutItem::BaseMinSize()
{
	fParent->_ValidateLayoutData();

	if (!fParent->Label())
		return BSize(-1,-1);
	
	return BSize(fParent->fLayoutData->labelWidth + 5,
		fParent->fLayoutData->labelHeight);
}


BSize
Spinner::LabelLayoutItem::BaseMaxSize()
{
	return BaseMinSize();	
}


BSize
Spinner::LabelLayoutItem::BasePreferredSize()
{
	return BaseMinSize();
}


BAlignment
Spinner::LabelLayoutItem::BaseAlignment()
{
	return BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_USE_FULL_HEIGHT);
}


status_t
Spinner::LabelLayoutItem::Archive(BMessage* into, bool deep) const
{
	BArchiver archiver(into);
	status_t err = BAbstractLayoutItem::Archive(into, deep);

	if (err == B_OK)
		err = into->AddRect(kFrameField, fFrame);

	return archiver.Finish(err);
}


BArchivable*
Spinner::LabelLayoutItem::Instantiate(BMessage* from)
{
	if (validate_instantiation(from, "Spinner::LabelLayoutItem"))
		return new LabelLayoutItem(from);
	return NULL;
}


Spinner::TextFieldLayoutItem::TextFieldLayoutItem(Spinner* parent)
	:
	fParent(parent),
	fFrame()
{
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
}


Spinner::TextFieldLayoutItem::TextFieldLayoutItem(BMessage* from)
	:
	BAbstractLayoutItem(from),
	fParent(NULL),
	fFrame()
{
	from->FindRect(kFrameField, &fFrame);
}


bool
Spinner::TextFieldLayoutItem::IsVisible()
{
	return !fParent->IsHidden(fParent);	
}


void
Spinner::TextFieldLayoutItem::SetVisible(bool visible)
{
}


BRect
Spinner::TextFieldLayoutItem::Frame()
{
	return fFrame;	
}


void
Spinner::TextFieldLayoutItem::SetFrame(BRect frame)
{
	fFrame = frame;
	fParent->_UpdateFrame();	
}


void
Spinner::TextFieldLayoutItem::SetParent(Spinner* parent)
{
	fParent = parent;	
}


BView*
Spinner::TextFieldLayoutItem::View()
{
	return fParent;
}


BSize
Spinner::TextFieldLayoutItem::BaseMinSize()
{
	fParent->_ValidateLayoutData();

	BSize size = fParent->fLayoutData->textFieldMin;
	size.width += B_V_SCROLL_BAR_WIDTH * 2;
	size.height += fParent->fTextControl->TextView()->LineHeight(0) + 4.0;
	return size;
}


BSize
Spinner::TextFieldLayoutItem::BaseMaxSize()
{
	return BaseMinSize();	
}


BSize
Spinner::TextFieldLayoutItem::BasePreferredSize()
{
	return BaseMinSize();
}


BAlignment
Spinner::TextFieldLayoutItem::BaseAlignment()
{
	return BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_USE_FULL_HEIGHT);
}


status_t
Spinner::TextFieldLayoutItem::Archive(BMessage* into, bool deep) const
{
	BArchiver archiver(into);
	status_t err = BAbstractLayoutItem::Archive(into, deep);

	if (err == B_OK)
		err = into->AddRect(kFrameField, fFrame);

	return archiver.Finish(err);
}


BArchivable*
Spinner::TextFieldLayoutItem::Instantiate(BMessage* from)
{
	if (validate_instantiation(from, "Spinner::TextFieldLayoutItem"))
		return new LabelLayoutItem(from);
	return NULL;
}


Spinner::Spinner(BRect frame, const char *name, const char *label, BMessage *msg,
				uint32 resize,uint32 flags)
	:
	BControl(frame,name,label,msg,resize,flags),
	fStep(1),
	fMin(0),
	fMax(100)
{
	_InitObject();
}


Spinner::Spinner(BMessage *data)
	:
	BControl(data)
{
	if (data->FindInt32("_min",&fMin) != B_OK)
		fMin = 0;
	if (data->FindInt32("_max",&fMax) != B_OK)
		fMin = 100;
	if (data->FindInt32("_step",&fStep) != B_OK)
		fMin = 1;
	_InitObject();
}


Spinner::Spinner(const char *name, const char *label, BMessage *msg
	,uint32 resize, uint32 flags)
	:
	BControl(BRect(0,0,100,15),name, label, msg, resize, flags),
	fStep(1),
	fMin(0),
	fMax(100)
{
	_InitObject();
}


Spinner::~Spinner(void)
{
	delete fPrivateData;
	delete fFilter;
}


void
Spinner::_InitObject(void)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	BRect r(Bounds());
	if (r.Height() < B_H_SCROLL_BAR_HEIGHT * 2)
		r.bottom = r.top + 1 + B_H_SCROLL_BAR_HEIGHT * 2;
	ResizeTo(r.Width(),r.Height());
	
	r.right -= B_V_SCROLL_BAR_WIDTH;
	
	font_height fh;
	BFont font;
	font.GetHeight(&fh);
	float textheight = fh.ascent + fh.descent + fh.leading;
	
	r.top = 0;
	r.bottom = textheight;
	
	fTextControl = new BTextControl(r,"textcontrol",Label(),"0",
									new BMessage(M_TEXT_CHANGED), B_FOLLOW_TOP | 
									B_FOLLOW_LEFT_RIGHT,
									B_WILL_DRAW | B_NAVIGABLE);
	AddChild(fTextControl);
	fTextControl->ResizeTo(r.Width(), MAX(textheight, fTextControl->TextView()->LineHeight(0) + 4.0));
	fTextControl->MoveTo(0,
		((B_H_SCROLL_BAR_HEIGHT * 2) - fTextControl->Bounds().Height()) / 2);
		
	fTextControl->SetDivider(StringWidth(Label()) + 5);
	
	BTextView *tview = fTextControl->TextView();
	tview->SetAlignment(B_ALIGN_LEFT);
	tview->SetWordWrap(false);

	BString string("QWERTYUIOP[]\\ASDFGHJKL;'ZXCVBNM,/qwertyuiop{}| "
		"asdfghjkl:\"zxcvbnm<>?!@#$%^&*()-_=+`~\r");

	for (int32 i = 0; i < string.CountChars(); i++) {
		char c = string.ByteAt(i);
		tview->DisallowChar(c);
	}

	r = Bounds();
	r.left = r.right - B_V_SCROLL_BAR_WIDTH;
	r.bottom = B_H_SCROLL_BAR_HEIGHT;

	r.OffsetBy(0,r.Height() + 1);
	fDownButton = new SpinnerArrowButton(r.LeftTop(),"down",ARROW_DOWN,
		r.Width()/2 + 1);
	AddChild(fDownButton);

	r.OffsetTo(r.right - B_V_SCROLL_BAR_WIDTH, B_H_SCROLL_BAR_HEIGHT/2 - 1);
	fUpButton = new SpinnerArrowButton(r.LeftTop(),"up",ARROW_UP,
		r.Width()/2 + 1);
	AddChild(fUpButton);

	fPrivateData = new SpinnerPrivateData;
	fFilter = new SpinnerMsgFilter;
}


BArchivable *
Spinner::Instantiate(BMessage *data)
{
	if (validate_instantiation(data, "Spinner"))
		return new Spinner(data);

	return NULL;
}


status_t
Spinner::Archive(BMessage *data, bool deep) const
{
	status_t status = BControl::Archive(data, deep);
	data->AddString("class","Spinner");
	
	if (status == B_OK)
		status = data->AddInt32("_min",fMin);
	
	if (status == B_OK)
		status = data->AddInt32("_max",fMax);
	
	if (status == B_OK)
		status = data->AddInt32("_step",fStep);
	
	return status;
}


status_t
Spinner::GetSupportedSuites(BMessage *msg)
{
	msg->AddString("suites","suite/vnd.DW-spinner");
	
	BPropertyInfo prop_info(sProperties);
	msg->AddFlat("messages",&prop_info);
	return BControl::GetSupportedSuites(msg);
}


BHandler*
Spinner::ResolveSpecifier(BMessage *msg, int32 index, BMessage *specifier,
									int32 form, const char *property)
{
	return BControl::ResolveSpecifier(msg, index, specifier, form, property);
}


void
Spinner::AttachedToWindow(void)
{
	Window()->AddCommonFilter(fFilter);
	fTextControl->SetTarget(this);
}


void
Spinner::DetachedFromWindow(void)
{
	Window()->RemoveCommonFilter(fFilter);
}


void
Spinner::SetValue(int32 value)
{
	if (value > GetMax() || value < GetMin())
		return;
	
	BControl::SetValue(value);
	
	char string[50];
	sprintf(string,"%ld",value);
	fTextControl->SetText(string);
}


void
Spinner::SetLabel(const char *text)
{
	fTextControl->SetLabel(text);
}


void
Spinner::ValueChanged(int32 value)
{
}


void
Spinner::MessageReceived(BMessage *msg)
{
	if (msg->what == M_TEXT_CHANGED) {
		BString string(fTextControl->Text());
		int32 newvalue = 0;
		
		sscanf(string.String(),"%ld",&newvalue);
		if (newvalue >= GetMin() && newvalue <= GetMax()) {
			// new value is in range, so set it and go
			SetValue(newvalue);
			Invoke();
			Invalidate();
			ValueChanged(Value());
		} else {
			// new value is out of bounds. Clip to range if current value is not
			// at the end of its range
			if (newvalue < GetMin() && Value() != GetMin()) {
				SetValue(GetMin());
				Invoke();
				Invalidate();
				ValueChanged(Value());
			} else if (newvalue>GetMax() && Value() != GetMax()) {
				SetValue(GetMax());
				Invoke();
				Invalidate();
				ValueChanged(Value());
			} else {
				char string[100];
				sprintf(string,"%ld",Value());
				fTextControl->SetText(string);
			}
		}
	}
	else
		BControl::MessageReceived(msg);
}


void
Spinner::GetPreferredSize(float *width, float *height)
{
	float w, h;
	
	// This code shamelessly copied from the Haiku TextControl.cpp and adapted
	
	// we need enough space for the label and the child text view
	font_height fontHeight;
	fTextControl->GetFontHeight(&fontHeight);
	float labelHeight = ceil(fontHeight.ascent + fontHeight.descent + fontHeight.leading);
	float textHeight = fTextControl->TextView()->LineHeight(0) + 4.0;

	h = max_c(labelHeight, textHeight);
	
	w = 25.0f + ceilf(fTextControl->StringWidth(fTextControl->Label())) + 
		ceilf(fTextControl->StringWidth(fTextControl->Text()));
	
	w += B_V_SCROLL_BAR_WIDTH;
	if (h < fDownButton->Frame().bottom)
		h = fDownButton->Frame().bottom;
	
	if (width)
		*width = w;
	if (height)
		*height = h;
}


void
Spinner::ResizeToPreferred(void)
{
	float w,h;
	GetPreferredSize(&w,&h);
	ResizeTo(w,h);
}


void
Spinner::_UpdateFrame()
{
	if (fLayoutData->labelLayoutItem == NULL
		|| fLayoutData->textFieldLayoutItem == NULL)
		return;

	BRect labelFrame = fLayoutData->labelLayoutItem->Frame();
	BRect textFrame = fLayoutData->textFieldLayoutItem->Frame();

	if (!labelFrame.IsValid() || !textFrame.IsValid())
		return;

	fDivider = textFrame.left - labelFrame.left;
	
	MoveTo(labelFrame.left, labelFrame.top);
	BSize oldSize = Bounds().Size();
	ResizeTo(textFrame.left + textFrame.Width() - labelFrame.left,
		textFrame.top + textFrame.Height() - labelFrame.top);
	BSize newSize = Bounds().Size();
	
	if (newSize != oldSize)
		Relayout();
}


void
Spinner::_ValidateLayoutData()
{
	if (fLayoutData->valid)
		return;
	font_height fontHeight = fLayoutData->fontInfo;
	GetFontHeight(&fontHeight);

	if (Label() != NULL) {
		fLayoutData->labelWidth = 25.0f + B_V_SCROLL_BAR_WIDTH
			+ ceilf(fTextControl->StringWidth(fTextControl->Label()));
		fLayoutData->labelHeight =  ceil(fontHeight.ascent
			+ fontHeight.descent + fontHeight.leading);
	} else {
		fLayoutData->labelWidth = 0;
		fLayoutData->labelHeight = 0;	
	}

	float divider = 0;
	if (fLayoutData->labelWidth > 0)
		divider = fLayoutData->labelWidth + 5;
		
	if ((Flags() & B_SUPPORTS_LAYOUT) == 0)
		divider = std::max(divider,fDivider);
	
	fLayoutData->textFieldHeight = fTextControl->MinSize();
	BSize min(fLayoutData->textFieldMin);
	min.height =  fTextControl->TextView()->LineHeight(0) + 4.0;
	min.width = 25.0f + ceilf(fTextControl->StringWidth(fTextControl->Text()));
	
	if (divider > 0)
		min.width += divider;
	if (fLayoutData->labelHeight > min.height)
		min.height = fLayoutData->labelHeight;
		
	fLayoutData->min = min;
	fLayoutData->valid = true;
	ResetLayoutInvalidation();
}


void
Spinner::SetSteps(int32 stepsize)
{
	fStep = stepsize;
}


void
Spinner::SetRange(int32 min, int32 max)
{
	SetMin(min);
	SetMax(max);
}


void
Spinner::GetRange(int32 *min, int32 *max)
{
	*min = fMin;
	*max = fMax;
}


void
Spinner::SetMax(int32 max)
{
	fMax = max;
	if (Value() > fMax)
		SetValue(fMax);
}


void
Spinner::SetMin(int32 min)
{
	fMin = min;
	if (Value() < fMin)
		SetValue(fMin);
}


void
Spinner::SetEnabled(bool value)
{
	if (IsEnabled() == value)
		return;
	
	BControl::SetEnabled(value);
	fTextControl->SetEnabled(value);
	fUpButton->SetEnabled(value);
	fDownButton->SetEnabled(value);
}


void
Spinner::SetDivider(float position)
{
	position = roundf(position);
	
	float delta = fDivider - position;
	if (delta == 0.0f)
		return;

	fDivider = position;

	BRect rect = fTextControl->TextView()->Frame();
	fTextControl->TextView()->MoveTo(_TextFieldOffset(), B_V_SCROLL_BAR_WIDTH);
	rect = rect | fTextControl->TextView()->Frame(); 
	
	Invalidate(rect);
}


float
Spinner::Divider() const
{
	return fDivider;
}


void
Spinner::MakeFocus(bool value)
{
	fTextControl->MakeFocus(value);
}


float
Spinner::_TextFieldOffset()
{
	return std::max(fDivider + B_V_SCROLL_BAR_WIDTH, B_V_SCROLL_BAR_WIDTH);
}


void
Spinner::DoLayout()
{
	if ((Flags() & B_SUPPORTS_LAYOUT) == 0)
		return;
	
	if (GetLayout()) {
		BView::DoLayout();
		return;	
	}
	
	_ValidateLayoutData();

	BSize size(Bounds().Size());
	if (size.width < fLayoutData->min.width)
		size.width = fLayoutData->min.width;
	if (size.height < fLayoutData->min.height)
		size.height = fLayoutData->min.height;

	float divider = 0;
	if (fLayoutData->labelLayoutItem != NULL
		&& fLayoutData->textFieldLayoutItem != NULL
		&& fLayoutData->labelLayoutItem->Frame().IsValid()
		&& fLayoutData->textFieldLayoutItem->Frame().IsValid()) {
		divider = fLayoutData->textFieldLayoutItem->Frame().left
			- fLayoutData->labelLayoutItem->Frame().left;	
	} else if (fLayoutData->labelWidth > 0)
		divider = fLayoutData->labelWidth + 4;
	
	BRect rect(fTextControl->TextView()->Frame());
	BRect textViewFrame(divider + B_V_SCROLL_BAR_WIDTH, B_V_SCROLL_BAR_WIDTH,
		size.width - B_V_SCROLL_BAR_WIDTH, size.height - B_V_SCROLL_BAR_WIDTH);

	BLayoutUtils::AlignInFrame(fTextControl->TextView(), textViewFrame);
	fDivider = divider;
	
	rect = rect | fTextControl->TextView()->Frame();
	Invalidate(rect);
}


BLayoutItem*
Spinner::CreateLabelLayoutItem()
{
	if (fLayoutData->labelLayoutItem == NULL)
		fLayoutData->labelLayoutItem = new LabelLayoutItem(this);
	return fLayoutData->labelLayoutItem;
}


BLayoutItem*
Spinner::CreateTextFieldLayoutItem()
{
	if (fLayoutData->textFieldLayoutItem == NULL)
		fLayoutData->textFieldLayoutItem = new TextFieldLayoutItem(this);
	return fLayoutData->textFieldLayoutItem;
}


int32
SpinnerPrivateData::ButtonRepeaterThread(void *data)
{
	Spinner *sp = (Spinner *)data;
	
	snooze(250000);
	
	bool exitval = false;
	
	sp->Window()->Lock();
	exitval = sp->fPrivateData->fExitRepeater;
	
	int32 scrollvalue = 0;
	if (sp->fPrivateData->fArrowDown == ARROW_UP)
		scrollvalue = sp->fStep;
	else if (sp->fPrivateData->fArrowDown != ARROW_NONE)
		scrollvalue = -sp->fStep;
	else
		exitval = true;
	
	sp->Window()->Unlock();
	
	while (!exitval) {
		sp->Window()->Lock();
		
		int32 newvalue = sp->Value() + scrollvalue;
		if (newvalue >= sp->GetMin() && newvalue <= sp->GetMax()) {
			// new value is in range, so set it and go
			sp->SetValue(newvalue);
			sp->Invoke();
			sp->Invalidate();
			sp->ValueChanged(sp->Value());
		} else {
			// new value is out of bounds. Clip to range if current value is not
			// at the end of its range
			if (newvalue<sp->GetMin() && sp->Value() != sp->GetMin()) {
				sp->SetValue(sp->GetMin());
				sp->Invoke();
				sp->Invalidate();
				sp->ValueChanged(sp->Value());
			} else  if (newvalue>sp->GetMax() && sp->Value() != sp->GetMax())
			{
				sp->SetValue(sp->GetMax());
				sp->Invoke();
				sp->Invalidate();
				sp->ValueChanged(sp->Value());
			}
		}
		sp->Window()->Unlock();
		
		snooze(50000);
		
		sp->Window()->Lock();
		exitval = sp->fPrivateData->fExitRepeater;
		sp->Window()->Unlock();
	}
	
	sp->Window()->Lock();
	sp->fPrivateData->fExitRepeater = false;
	sp->fPrivateData->fRepeaterID = -1;
	sp->Window()->Unlock();
	return 0;
	exit_thread(0);
}


SpinnerArrowButton::SpinnerArrowButton(BPoint location, const char *name,
										arrow_direction dir, float height)
 :BView(BRect(0,0,height*2,height).OffsetToCopy(location),
 		name, B_FOLLOW_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW)
{
	fDirection = dir;
	fEnabled = true;
	fHeight = height;
	fMouseDown = false;
	fMouseOver = false;
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}


SpinnerArrowButton::~SpinnerArrowButton(void)
{
}


void
SpinnerArrowButton::MouseDown(BPoint pt)
{
	if (fEnabled == false)
		return;
	fParent->MakeFocus(true);
	MouseDownThread<SpinnerArrowButton>::TrackMouse(this, 
		&SpinnerArrowButton::_DoneTracking, &SpinnerArrowButton::_Track);
}


void
SpinnerArrowButton::_ModifyValue()
{
	int32 step = fParent->GetSteps();
	int32 newvalue = fParent->Value();

	if (fDirection == ARROW_UP) {
		fParent->fPrivateData->fArrowDown = ARROW_UP;
		newvalue += step;
	} else {
		fParent->fPrivateData->fArrowDown = ARROW_DOWN;
		newvalue -= step;
	}	

	if (newvalue >= fParent->GetMin() && newvalue <= fParent->GetMax()) {
		// new value is in range, so set it and go
		fParent->SetValue(newvalue);
		fParent->Invoke();
		fParent->ValueChanged(fParent->Value());
	} else {
		// new value is out of bounds. Clip to range if current value is not
		// at the end of its range
		if (newvalue < fParent->GetMin() && 
			fParent->Value() != fParent->GetMin()) {
			fParent->SetValue(fParent->GetMin());
			fParent->Invoke();
			fParent->ValueChanged(fParent->Value());
		} else if (newvalue>fParent->GetMax() && 
			fParent->Value() != fParent->GetMax()) {
			fParent->SetValue(fParent->GetMax());
			fParent->Invoke();
			fParent->ValueChanged(fParent->Value());
		} else {
			// cases which go here are if new value is <minimum and value already at
			// minimum or if > maximum and value already at maximum
			return;
		}
	}
}

void
SpinnerArrowButton::_DoneTracking(BPoint point)
{
	if (!Bounds().Contains(point) || fMouseDown) {
		fMouseDown = false;
		return;
	}
}


void
SpinnerArrowButton::_Track(BPoint point, uint32)
{
	if (Bounds().Contains(point)) {
		fMouseDown = true;
		_ModifyValue();
	} else
		fMouseDown = false;

	Invalidate();
}


void
SpinnerArrowButton::MouseUp(BPoint pt)
{
	fParent->MakeFocus(false);

	if (fEnabled) {
		fMouseDown = false;

		if (fParent) {
			fParent->fPrivateData->fArrowDown = ARROW_NONE;
			fParent->fPrivateData->fExitRepeater = true;
		}
		Invalidate();
	}
}


void
SpinnerArrowButton::MouseMoved(BPoint pt, uint32 transit, const BMessage *msg)
{
	if (fEnabled == false)
		return;

	if (transit == B_ENTERED_VIEW || transit == B_INSIDE_VIEW) {
		BPoint point;
		uint32 buttons;
		GetMouse(&point,&buttons);
		if (buttons == 0 && Bounds().Contains(point))
			fMouseOver = true;
		else
			fMouseOver = false;
		if(!fMouseDown)
			Invalidate();
	}

	if (transit == B_EXITED_VIEW || transit == B_OUTSIDE_VIEW) {
		fMouseOver = false;
		MouseUp(Bounds().LeftTop());
	}
}


void
SpinnerArrowButton::Draw(BRect update)
{
	PushState();
	rgb_color c = ui_color(B_PANEL_BACKGROUND_COLOR);
	BRect r(Bounds());
	r.OffsetBy(-1,0);

	float tint;

	if (!fEnabled)
		tint = B_DARKEN_1_TINT;
	else if (fMouseDown)
		tint = B_DARKEN_MAX_TINT;	
	else if (fMouseOver)
		tint = B_DARKEN_3_TINT;
	else
		tint = B_DARKEN_2_TINT;

	SetHighColor(tint_color(c, tint));
	StrokeRect(r);

	if (fDirection == ARROW_UP) {
		be_control_look->DrawArrowShape(this,r,update,c,
			BControlLook::B_UP_ARROW,0,tint);
	} else {
		be_control_look->DrawArrowShape(this,r,update,c,
			BControlLook::B_DOWN_ARROW,0,tint);
	}
	PopState();
}


void
SpinnerArrowButton::AttachedToWindow(void)
{
	fParent = (Spinner*)Parent();
}


void
SpinnerArrowButton::DetachedToWindow(void)
{
	fParent = NULL;
}


void
SpinnerArrowButton::SetEnabled(bool value)
{
	fEnabled = value;
	Invalidate();
}


SpinnerMsgFilter::SpinnerMsgFilter(void)
 : BMessageFilter(B_PROGRAMMED_DELIVERY, B_ANY_SOURCE,B_KEY_DOWN)
{
}


SpinnerMsgFilter::~SpinnerMsgFilter(void)
{
}


filter_result
SpinnerMsgFilter::Filter(BMessage *msg, BHandler **target)
{
	int32 c;
	msg->FindInt32("raw_char",&c);
	switch (c) {
		case B_ENTER: {
			BTextView *text = dynamic_cast<BTextView*>(*target);
			if (text && text->IsFocus()) {
				BView *view = text->Parent();
				while (view) {					
					Spinner *spin = dynamic_cast<Spinner*>(view);
					if (spin) {
						BString string(text->Text());
						int32 newvalue = 0;
						
						sscanf(string.String(),"%ld",&newvalue);
						if (newvalue != spin->Value()) {
							spin->SetValue(newvalue);
							spin->Invoke();
						}
						return B_SKIP_MESSAGE;
					}
					view = view->Parent();
				}
			}
			return B_DISPATCH_MESSAGE;
		}
		case B_TAB: {
			// Cause Tab characters to perform keybaord navigation
			BHandler *h = *target;
			BView *v = NULL;
			
			h = h->NextHandler();
			while (h) {
				v = dynamic_cast<BView*>(h);
				if (v) {
					*target = v->Window();
					return B_DISPATCH_MESSAGE;
				}
				h = h->NextHandler();
			}
			return B_SKIP_MESSAGE;
		}
		case B_UP_ARROW:
		case B_DOWN_ARROW: {
			BTextView *text = dynamic_cast<BTextView*>(*target);
			if (text && text->IsFocus()) {
				// We have a text view. See if it currently has the focus and belongs
				// to a Spinner control. If so, change the value of the spinner
				
				// TextViews are complicated beasts which are actually multiple views.
				// Travel up the hierarchy to see if any of the target's ancestors are
				// a Spinner.
				
				BView *view = text->Parent();
				while (view) {					
					Spinner *spin = dynamic_cast<Spinner*>(view);
					if (spin) {
						int32 step = spin->GetSteps();
						if (c == B_DOWN_ARROW)
							step = 0 - step;
						
						spin->SetValue(spin->Value() + step);
						spin->Invoke();
						return B_SKIP_MESSAGE;
					}
					view = view->Parent();
				}
			}
			
			return B_DISPATCH_MESSAGE;
		}
		default:
			return B_DISPATCH_MESSAGE;
	}
	
	// shut the stupid compiler up
	return B_SKIP_MESSAGE;
}
