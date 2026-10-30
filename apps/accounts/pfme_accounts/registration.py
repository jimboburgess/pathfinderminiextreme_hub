from django.shortcuts import render,redirect
from django.http import HttpResponse
from django.views.decorators.http import require_http_methods
from django.core.exceptions import ValidationError
from django.db import IntegrityError,OperationalError
from .forms import InvitationRegistrationForm
from .invitations import claim
from .views import limited


@require_http_methods(['GET','POST'])
def register(request):
    form=InvitationRegistrationForm(request.POST if request.method=='POST' else None)
    status=200
    if request.method=='POST':
        if limited('invite-claim-global',20):return HttpResponse('Please wait a minute before trying again.',status=429)
        if form.is_valid():
            try:
                claim(form.cleaned_data['token'],form.cleaned_data['username'],form.cleaned_data['password1'])
                return redirect('/accounts/login/?registered=1')
            except ValidationError as error:form.add_error(None,error)
            except IntegrityError:form.add_error(None,'That username is unavailable. Please choose another.')
            except OperationalError:form.add_error(None,'Registration is busy. Please retry shortly.')
        status=400
    response=render(request,'register.html',{'form':form,'posted':request.method=='POST'},status=status)
    response['Referrer-Policy']='no-referrer'
    return response


def registration_script(request):
    return HttpResponse("""'use strict';
(() => {
 const form=document.getElementById('registration'),note=document.getElementById('invite-note');
 const token=location.hash.slice(1);
 if(location.hash)history.replaceState(null,'',location.pathname);
 if(/^[0-9a-f]{64}$/.test(token)){form.elements.token.value=token;form.hidden=false;note.textContent='Choose your own account details.';}
})();
""",content_type='application/javascript')
