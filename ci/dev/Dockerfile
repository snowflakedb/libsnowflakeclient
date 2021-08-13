ARG BASE_IMAGE_NAME
FROM $BASE_IMAGE_NAME
ARG LOCAL_USER_ID


# replace outdated yum repo link with snowflake internal links
COPY CentOS-Base.repo /etc/yum.repos.d/
CMD chmod 644 /etc/yum.repos.d/CentOS-Base.repo
RUN yum-config-manager --disable centos*
RUN yum-config-manager --disable WANdisco-git
RUN yum-config-manager --disable okay
RUN yum-config-manager --disable base
RUN yum-config-manager --disable updates
RUN yum-config-manager --enable base
RUN yum-config-manager --enable updates
RUN yum-config-manager --enable extras

# setup ssh server for debugging
RUN yum install -y openssh-server

RUN ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key -N ''
RUN ssh-keygen -t dsa -f /etc/ssh/ssh_host_dsa_key -N ''

RUN mkdir /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

# 22 for ssh server. 7777 for gdb server.
EXPOSE 22 7777

RUN useradd --shell /bin/bash -c "" -m debugger
RUN echo 'debugger:pwd' | chpasswd

# let cmake link to cmake3
RUN ln -s /usr/bin/cmake3 /usr/bin/cmake

# gosu does not works well with ssh. Override the Build image settings
COPY entrypoint.sh /usr/bin/entrypoint.sh
RUN chmod +x /usr/bin/entrypoint.sh
ENTRYPOINT [ "/usr/bin/entrypoint.sh" ]

CMD ["/usr/sbin/sshd", "-D"]
