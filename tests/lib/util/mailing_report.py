import smtplib

def sysbenchSendMail(to_address,message):
    """mails the report of sysbench test to the specified reciepients"""

    #configuring sender's login
    from_address='smtplibpython@gmail.com'
    from_password='smtpprotocol'

    #sending mail to specified to_addresses
    print "mailing sysbench report..."
    server=smtplib.SMTP('smtp.gmail.com',587)
    server.ehlo()
    server.starttls()
    server.ehlo()
    server.login(from_address,from_password)
    server.sendmail(from_address,to_address,message)
    print "sysbench report successfully sent..."
    server.close()
