from django import forms
from django.contrib.auth.forms import AuthenticationForm


class PlayerAuthenticationForm(AuthenticationForm):
    remember_me = forms.BooleanField(
        required=False,
        label="Remember me for 7 days",
        help_text="Use only on your own device. Leave unchecked for a 4-hour login.",
    )
