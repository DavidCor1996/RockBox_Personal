"""Contact book and monthly conversation editor for the iPod Messenger app."""
import calendar
import uuid
from datetime import datetime
from pathlib import Path
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QWidget,QVBoxLayout,QHBoxLayout,QFormLayout,QLabel,QPushButton,QLineEdit,
    QComboBox,QSpinBox,QPlainTextEdit,QTableWidget,QTableWidgetItem,QFileDialog,
    QMessageBox,QTabWidget,QDialog,QDialogButtonBox,QDateEdit,QTimeEdit,
)
from services.msn_app import compile_schedule
from ui.twitter_panel import TwitterJob
from PySide6.QtCore import QThreadPool


class MsnPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service,self.device_provider=service,device_provider
        self.plan=service.load()
        self.job=None
        layout=QVBoxLayout(self)
        title=QLabel('MSN Messenger')
        title.setStyleSheet('font-size:24px; font-weight:bold; color:#234e88')
        layout.addWidget(title)
        note=QLabel('Build your contact list and a month of offline conversations. Messages arrive within your chosen hours, with typing pauses and original MSN sounds. Replies stay on your iPod.')
        note.setWordWrap(True)
        layout.addWidget(note)
        row=QHBoxLayout()
        self.mode=QComboBox();self.mode.addItem('30 days from first sync','fresh_sync');self.mode.addItem('Calendar month','calendar')
        self.mode.setCurrentIndex(max(0,self.mode.findData(self.plan.get('schedule_mode','calendar'))));row.addWidget(self.mode)
        row.addWidget(QLabel('Month'))
        self.month=QLineEdit(self.plan['month'])
        self.month.setPlaceholderText('YYYY-MM')
        self.month.setMaximumWidth(110)
        row.addWidget(self.month)
        for label,handler in [('Save month',self.save_month),('Preview delivery times',self.preview),('Sync Messenger',self.sync)]:
            b=QPushButton(label);b.clicked.connect(handler);row.addWidget(b)
        layout.addLayout(row)
        tabs=QTabWidget();layout.addWidget(tabs)
        contacts=QWidget();cl=QVBoxLayout(contacts)
        self.contacts=QTableWidget(0,4);self.contacts.setHorizontalHeaderLabels(['Name','Personal message','Email','Display picture']);self.contacts.setSelectionBehavior(QTableWidget.SelectRows);self.contacts.setEditTriggers(QTableWidget.NoEditTriggers);cl.addWidget(self.contacts)
        row=QHBoxLayout()
        for label,handler in [('Add contact',lambda:self.contact_dialog()),('Edit contact',self.edit_contact),('Remove contact',self.remove_contact),('Import contacts CSV',self.import_contacts),('Import Instagram profile',self.import_instagram)]:
            b=QPushButton(label);b.clicked.connect(handler);row.addWidget(b)
        cl.addLayout(row);tabs.addTab(contacts,'Contacts')
        messages=QWidget();ml=QVBoxLayout(messages)
        self.messages=QTableWidget(0,5);self.messages.setHorizontalHeaderLabels(['Contact','Days','Hours','Type','Message / attachment']);self.messages.setSelectionBehavior(QTableWidget.SelectRows);self.messages.setEditTriggers(QTableWidget.NoEditTriggers);ml.addWidget(self.messages)
        for table in (self.contacts,self.messages):
            table.setSelectionMode(QTableWidget.SingleSelection)
            table.setAlternatingRowColors(True)
            table.setStyleSheet('QTableWidget::item:selected { background:#316ac5; color:white; }')
        self.contacts.cellDoubleClicked.connect(lambda row,column:self.contact_dialog(row))
        self.messages.cellDoubleClicked.connect(lambda row,column:self.message_dialog(row))
        row=QHBoxLayout()
        for label,handler in [('Add conversation',lambda:self.message_dialog()),('Add photo',lambda:self.message_dialog(initial_kind='photo')),('Edit conversation',self.edit_message),('Remove conversation',self.remove_message)]:
            b=QPushButton(label);b.clicked.connect(handler);row.addWidget(b)
        ml.addLayout(row);tabs.addTab(messages,'Monthly conversations')
        self.status=QLabel('');self.status.setWordWrap(True);layout.addWidget(self.status)
        self.refresh()

    def refresh(self):
        self.plan=self.service.load();self.month.setText(self.plan['month'])
        self.mode.setCurrentIndex(max(0,self.mode.findData(self.plan.get('schedule_mode','calendar'))))
        self.contacts.setRowCount(len(self.plan['contacts']))
        for i,c in enumerate(self.plan['contacts']):
            for j,value in enumerate([c['name'],c.get('status',''),c.get('email',''),Path(c.get('pfp','')).name if c.get('pfp') else 'Original MSN picture']):
                self.contacts.setItem(i,j,QTableWidgetItem(value))
        names={c['id']:c['name'] for c in self.plan['contacts']}
        self.messages.setRowCount(len(self.plan['messages']))
        for i,m in enumerate(self.plan['messages']):
            for j,value in enumerate([names.get(m['contact'],''),f"{m['day']}–{m['last_day']}",f"{m['after']}–{m['before']}",'conversation' if m.get('parts') else m['kind'],' / '.join(p.get('text') or Path(p.get('media','')).name for p in m['parts']) if m.get('parts') else m.get('text') or Path(m.get('media','')).name]):
                self.messages.setItem(i,j,QTableWidgetItem(value))
        self.contacts.resizeColumnsToContents();self.messages.resizeColumnsToContents()

    def persist(self):
        try:self.service.save(self.plan);self.refresh();return True
        except Exception as exc:QMessageBox.warning(self,'Messenger',str(exc));self.plan=self.service.load();return False

    def save_month(self):
        self.plan['month']=self.month.text().strip();self.plan['schedule_mode']=self.mode.currentData();self.persist()

    def contact_dialog(self, index=None):
        c=dict(self.plan['contacts'][index]) if index is not None else {'id':uuid.uuid4().hex[:16]}
        d=QDialog(self);d.setWindowTitle('Messenger contact');f=QFormLayout(d)
        fields={}
        for key,label in [('name','Display name'),('email','Email (optional)'),('status','Personal message'),('pfp','Display picture')]:
            fields[key]=QLineEdit(c.get(key,''));f.addRow(label,fields[key])
        b=QPushButton('Choose picture…');b.clicked.connect(lambda:self.pick_file(fields['pfp'],'Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp)'));f.addRow(b)
        buttons=QDialogButtonBox(QDialogButtonBox.Save|QDialogButtonBox.Cancel);buttons.accepted.connect(d.accept);buttons.rejected.connect(d.reject);f.addRow(buttons)
        if d.exec()==QDialog.Accepted:
            c.update({k:v.text().strip() for k,v in fields.items()})
            if not c['name']:return
            if index is None:self.plan['contacts'].append(c)
            else:self.plan['contacts'][index]=c
            self.persist()

    def edit_contact(self):
        i=self.contacts.currentRow()
        if i>=0:self.contact_dialog(i)

    def remove_contact(self):
        i=self.contacts.currentRow()
        if i<0:return
        cid=self.plan['contacts'][i]['id']
        if any(m['contact']==cid for m in self.plan['messages']):
            QMessageBox.information(self,'Messenger','Remove this contact’s scheduled conversations first.');return
        self.plan['contacts'].pop(i);self.persist()

    def import_contacts(self):
        path,_=QFileDialog.getOpenFileName(self,'Contact CSV','','CSV (*.csv)')
        if path:
            try:self.service.import_contacts(path);self.refresh()
            except Exception as exc:QMessageBox.warning(self,'Messenger',str(exc))

    def import_instagram(self):
        from PySide6.QtWidgets import QInputDialog
        value,ok=QInputDialog.getText(self,'Instagram contact','Profile URL or username:')
        if ok and value.strip():
            self.start_job(self.service.import_instagram,value.strip())

    def message_dialog(self,index=None,initial_kind='message'):
        if not self.plan['contacts']:return
        m=dict(self.plan['messages'][index]) if index is not None else {'id':uuid.uuid4().hex[:16],'kind':initial_kind}
        d=QDialog(self);d.setWindowTitle('Schedule a conversation');d.resize(580,540);f=QFormLayout(d)
        contact=QComboBox()
        for c in self.plan['contacts']:contact.addItem(c['name'],c['id'])
        if m.get('contact'):contact.setCurrentIndex(max(0,contact.findData(m['contact'])))
        f.addRow('Contact',contact)
        kind=QComboBox();kind.addItems(['message','photo','gif','video','nudge','status','wink']);kind.setCurrentText(m.get('kind','message'));f.addRow('Send',kind)
        first,last=QSpinBox(),QSpinBox()
        date=datetime.strptime(self.plan['month'],'%Y-%m');days=30 if self.plan.get('schedule_mode')=='fresh_sync' else calendar.monthrange(date.year,date.month)[1]
        for s in (first,last):s.setRange(1,days)
        first.setValue(m.get('day',1));last.setValue(m.get('last_day',days));f.addRow('Earliest day',first);f.addRow('Latest day',last)
        after,before=QLineEdit(m.get('after','18:00')),QLineEdit(m.get('before','21:00'));f.addRow('Not before (HH:MM)',after);f.addRow('Not after (HH:MM)',before)
        parts=[dict(p) for p in m.get('parts',[])]
        if not parts:
            parts=[dict(kind=m.get('kind','message'),text=t,media=m.get('media','')) for t in m.get('text','').splitlines() or ['']]
        table=QTableWidget(0,3);table.setHorizontalHeaderLabels(['Type','Message / caption','Attachment']);table.setSelectionBehavior(QTableWidget.SelectRows);f.addRow('Conversation sequence',table)
        def add_part(part=None):
            part=part or {};i=table.rowCount();table.insertRow(i)
            combo=QComboBox();combo.addItems(['message','photo','gif','video','nudge','status','wink']);combo.setCurrentText(part.get('kind','message'));table.setCellWidget(i,0,combo)
            table.setItem(i,1,QTableWidgetItem(part.get('text','')));table.setItem(i,2,QTableWidgetItem(part.get('media','')));table.setCurrentCell(i,1)
        for part in parts:add_part(part)
        table.setColumnWidth(1,280)
        controls=QHBoxLayout()
        def choose_media():
            i=table.currentRow()
            if i<0:return
            path,_=QFileDialog.getOpenFileName(d,'Choose media','','Media (*.png *.jpg *.jpeg *.gif *.bmp *.webp *.mp4 *.mov *.m4v *.mpg *.mpeg *.webm)')
            if path:
                table.item(i,2).setText(path);ext=Path(path).suffix.lower();table.cellWidget(i,0).setCurrentText('gif' if ext=='.gif' else 'video' if ext in ('.mp4','.mov','.m4v','.mpg','.mpeg','.webm') else 'photo')
        for label,handler in [('Add message',lambda:add_part()),('Remove',lambda:table.removeRow(table.currentRow())),('Attach media…',choose_media)]:
            button=QPushButton(label);button.clicked.connect(handler);controls.addWidget(button)
        f.addRow(controls)
        chooser=QComboBox()
        def populate():
            chooser.clear();chooser.addItem('Add an imported photo or video…',None)
            for item in next((c.get('media_library',[]) for c in self.plan['contacts'] if c['id']==contact.currentData()),[]):
                chooser.addItem(item.get('caption','')[:70] or Path(item['path']).name,item)
        def select_media():
            item=chooser.currentData()
            if isinstance(item,dict):add_part(dict(kind=item.get('kind','photo'),media=item['path'],text=''));chooser.setCurrentIndex(0)
        populate();contact.currentIndexChanged.connect(populate);chooser.currentIndexChanged.connect(select_media);f.addRow('Imported media',chooser)
        def choose_download():
            from PySide6.QtWidgets import QInputDialog
            items=self.service.imported_media()
            if not items:
                QMessageBox.information(d,'Imported media','Import media with the existing Instagram or OnlyFans sync tool first.');return
            labels=[f"{i+1}. {item['label']}" for i,item in enumerate(items)]
            label,ok=QInputDialog.getItem(d,'Choose imported media','Existing local downloads:',labels,0,False)
            if ok:
                item=items[labels.index(label)]
                add_part(dict(kind=item['kind'],media=item['path'],text=''))
        downloads=QPushButton('Choose from Instagram / OnlyFans downloads…')
        downloads.clicked.connect(choose_download);f.addRow(downloads)
        winks=QComboBox();winks.addItem('Add an original MSN Wink…',None)
        catalog=self.service.repo_root/'assets/ipodjs/rockbox/msn/winks/catalog.json'
        if catalog.exists():
            import json
            for wink in json.loads(catalog.read_text()):winks.addItem(wink['name'],wink['id'])
        def select_wink():
            if winks.currentData():add_part(dict(kind='wink',media=winks.currentData(),text=winks.currentText()));winks.setCurrentIndex(0)
        winks.currentIndexChanged.connect(select_wink);f.addRow('Original Winks',winks)
        options=QPlainTextEdit('\n'.join(x['text']+' | '+x.get('response','') for x in m.get('reply_options',[])))
        options.setPlaceholderText('One choice per line: Your reply | Contact’s follow-up');options.setMaximumHeight(95);f.addRow('Natural reply choices',options)
        note=QLabel('Rows arrive in order with natural pauses. Use MSN shortcuts such as :) or paste emoji, including 🌷. These are authored offline conversations. Re-syncing preserves delivery times and history.');note.setWordWrap(True);f.addRow(note)
        buttons=QDialogButtonBox(QDialogButtonBox.Save|QDialogButtonBox.Cancel);buttons.accepted.connect(d.accept);buttons.rejected.connect(d.reject);f.addRow(buttons)
        if d.exec()==QDialog.Accepted:
            m.update(contact=contact.currentData(),kind=kind.currentText(),day=first.value(),last_day=last.value(),after=after.text(),before=before.text(),parts=[dict(kind=table.cellWidget(i,0).currentText(),text=table.item(i,1).text(),media=table.item(i,2).text()) for i in range(table.rowCount())])
            m['reply_options']=[dict(zip(('text','response'),(row.split('|',1)+[''])[:2])) for row in options.toPlainText().splitlines() if row.strip()]
            m.pop('text',None);m.pop('media',None)
            if index is None:self.plan['messages'].append(m)
            else:self.plan['messages'][index]=m
            self.persist()

    def edit_message(self):
        i=self.messages.currentRow()
        if i>=0:self.message_dialog(i)

    def remove_message(self):
        i=self.messages.currentRow()
        if i>=0:self.plan['messages'].pop(i);self.persist()

    def pick_file(self,field,filters):
        path,_=QFileDialog.getOpenFileName(self,'Choose file','',filters)
        if path:field.setText(path)

    def preview(self):
        try:
            rows=compile_schedule(self.plan);names={c['id']:c['name'] for c in self.plan['contacts']}
            d=QDialog(self);d.setWindowTitle('Monthly delivery preview');d.resize(750,500);layout=QVBoxLayout(d);text=QPlainTextEdit();text.setReadOnly(True)
            text.setPlainText('\n'.join(f"{datetime.utcfromtimestamp(e['at']):%b %d %H:%M:%S}  {names[e['contact']]}  [{e['kind']}] {e['text']}" for e in rows));layout.addWidget(text);d.exec()
        except Exception as exc:QMessageBox.warning(self,'Messenger',str(exc))

    def start_job(self,function,*args):
        if self.job:return
        self.setEnabled(False);self.status.setText('Preparing Messenger…')
        self.job=TwitterJob(function,*args)
        self.job.signals.progress.connect(self.status.setText)
        self.job.signals.finished.connect(self.finished)
        self.job.signals.error.connect(self.failed)
        QThreadPool.globalInstance().start(self.job)

    def finished(self,result):
        self.job=None;self.setEnabled(True);self.refresh()
        if 'month' in result:
            month=datetime.strptime(result['month'],'%Y-%m').strftime('%B %Y')
            period='30 days from first sync' if self.plan.get('schedule_mode')=='fresh_sync' else month
            self.status.setText(f"Messenger synced: {result['contacts']} contacts and {result['messages']} messages; {period}. Existing history is preserved.")
        else:
            self.status.setText(f"Added {result.get('contact','contact')}. {result.get('available_media',0)} photos and videos available to schedule.")

    def failed(self,error):
        self.job=None;self.setEnabled(True);self.status.setText(error)

    def sync(self):
        device=self.device_provider()
        if not device or not getattr(device,'mount_path',''):
            QMessageBox.information(self,'Messenger','Connect an iPod or select a simulator device first.');return
        self.start_job(self.service.sync,device.mount_path)
