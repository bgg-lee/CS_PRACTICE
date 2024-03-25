#!/usr/bin/env python
# coding: utf-8

# # Q1. Beef Roasting

# ### Referring to a beef temperature chart below, write a code that takes internal_temperature as an input and outputs the roasted degree.

# ![beef.png](attachment:beef.png)

# In[8]:


def beef(temp : int) -> str:
    
    if temp >= 45 and temp <=47:
        return "Rare"
    elif temp >= 50 and temp <=52:
        return "Medium_Rare"
    elif temp >=55 and temp <=60:
        return "Medium"
    elif temp >= 64 and temp <= 70:
        return "Well_done"
    else:
        return "OutOfRange"


# In[9]:


print(beef(44),beef(45),beef(50),beef(56),beef(69),beef(80))


# # Q2. Name Separator

# ### Taking a name as input, write a function that separates last name and first name.

# In[10]:


"A B".split(' ')


# In[37]:


def name(fullname : str) -> str :
    first_name = fullname.split(" ")[0]
    last_name = fullname.split(" ")[1]
    return print("first name:",first_name,", last name:",last_name)


# In[38]:


name("Michael Jackson")


# In[39]:


name("Jaemyeong Park")


# # Q3. Age and Gender Calculator

# ### Taking the first seven digits of the residential number as input values, write a function that outputs the age and gender.

# **Guidelines** 
# 1) Distinguish gender (hint: 7th digit: 1 or 3- male, 2 or 4-female) \
# 2) Calculate the age based on today's date (hint:compare birthday and today's date)

# In[101]:


from datetime import date
today = date.today()
# today = today.strftime("%Y%m%d")

def cal(num:int)->str:
    gender = "gender"
    if num%10 == 1 or num%10 == 3 :
        gender = 'male'
    elif num%10 == 2 or num%10 == 4 :
        gender = 'female'
    if num%10 == 1 or num%10 == 2:
        num = num//10 + 19000000
    elif num%10 == 3 or num%10 == 4:
        num = num//10 + 20000000
    
    # 만 나이로 계산
    num_date = date(int(str(num)[:4]),int(str(num)[4:6]),
                    int(str(num)[6:]))
    age = ((today - num_date)/365).days
    
    return(gender,age)  


# In[97]:


cal(9807231)


# In[103]:


cal(9509142)


# In[102]:


cal(9805032)


# In[110]:


cal(000520)


# In[ ]:


# 2000년대 생은 모르겠다.. 


# In[117]:


# other solution

from datetime import date
today = date.today()
today = today.strftime("%Y%m%d")
## int -> String
    

def q3(residential_number):
        
    if residential_number[-1] in ('1', '3'):
        gender = 'male'
    else:
        gender = 'female'
        
    if residential_number[-1] in ('1', '2'):
        birth_year = '19'+residential_number[:2]
    else:
        birth_year = '20'+residential_number[:2]
    
    age = int(today[:4]) - int(birth_year)
    
    print('gender:', gender, ', age:', age)
    
#for test 
my_number = input("Write your residential number:(eg.220908-2)  ")

q3(my_number)


# In[ ]:





# In[ ]:





# In[ ]:





# ### Sketch book

# In[79]:


age = 19950913
print(str(age)[:4],str(age)[4:6],str(age)[6:])
int(str(age)[4:6])


# In[54]:


from datetime import date
test = date.today()


# In[70]:


# test1=19950913
test1=date(1995,9,13)
test2=test-test1
(test2/365).days


# In[46]:


today


# In[52]:


int(today)


# In[48]:


today


# In[ ]:





# In[ ]:





# In[ ]:





# In[ ]:




